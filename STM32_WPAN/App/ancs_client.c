/**
 * ancs_client.c - BLE_AutoLock Step 1c: ANCS subscription = iOS keep-alive
 *
 * The iPhone is the GATT *server* for ANCS, our board is the *client*.
 * One GATT procedure at a time; each ends with ACI_GATT_PROC_COMPLETE:
 *
 *   1. find ANCS service on the phone          aci_gatt_disc_primary_service_by_uuid
 *   2. find "Notification Source" char         aci_gatt_disc_all_char_of_service
 *   3. find its CCCD (0x2902)                  aci_gatt_disc_all_char_desc
 *   4. write 01 00 to CCCD (notifications on)  aci_gatt_write_char_value
 *        -> "insufficient authentication/encryption" if not paired yet:
 *           security request, wait for pairing, retry every 3 s
 *   5. READY: iOS now keeps the link alive
 *
 * Same design as ams_client.c in BLE_TFT.
 */
#include "main.h"
#include "app_common.h"
#include "log_module.h"
#include "screen_log.h"   /* BLE_TFT_AutoLock: log goes to the TFT */
#include "ble_core.h"
#include "svc_ctl.h"
#include "ancs_client.h"
#include <string.h>

/* 128-bit UUIDs, least significant byte first */
static const uint8_t UUID_ANCS[16] = { 0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
                                       0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79 }; /* 7905F431-B5CE-4E99-A40F-4B1E122D00D0 */
static const uint8_t UUID_NS[16]   = { 0xBD, 0x1D, 0xA2, 0x99, 0xE6, 0x25, 0x58, 0x8C,
                                       0xD9, 0x42, 0x01, 0x63, 0x0D, 0x12, 0xBF, 0x9F }; /* Notification Source 9FBF120D-6301-42D9-8C58-25E699A21DBD */

#define ATT_ERR_INSUF_AUTHENTICATION  0x05
#define ATT_ERR_INSUF_ENCRYPTION      0x0F
#define NO_CONN                       0xFFFFU

typedef enum {
  S_IDLE, S_WAIT, S_DISC_SVC, S_DISC_CHAR, S_DISC_DESC, S_SUBSCRIBE, S_WAIT_PAIR,
  S_READY, S_NOT_FOUND, S_ERROR
} step_t;

static const char *const step_name[] = {
  "IDLE", "WAIT", "DISC_SVC", "DISC_CHAR", "DISC_DESC", "SUBSCRIBE", "WAIT_PAIR",
  "READY", "NOT_FOUND", "ERROR"
};

static step_t   step = S_IDLE;
static uint8_t  proc_busy;           /* a GATT procedure of ours is running */
static uint8_t  att_error;           /* ATT error code of that procedure    */
static uint16_t conn = NO_CONN;
static uint16_t svc_start, svc_end;
static uint16_t ns_handle, ns_cccd;
static uint8_t  ticks;               /* 500 ms ticks for waits/retries      */
static uint8_t  tries;
static uint8_t  retry_pending;

static void set_step(step_t s)
{
  step = s;
  SLOG(">> ancs: %s\n", step_name[s]);
}

/* 1 = procedure started; otherwise retried on the next tick (stack busy) */
static uint8_t start_proc(tBleStatus st)
{
  if (st == BLE_STATUS_SUCCESS)
  {
    proc_busy = 1;
    att_error = 0;
    retry_pending = 0;
    return 1;
  }
  retry_pending = 1;
  if (++tries > 60)                  /* ~30 s of "busy" */
  {
    SLOG(">> ancs: command keeps failing 0x%02X\n", st);
    set_step(S_ERROR);
  }
  return 0;
}

static void do_step(void)
{
  static const uint8_t cccd_on[2] = { 0x01, 0x00 };

  switch (step)
  {
    case S_DISC_SVC:
    {
      UUID_t u;
      memcpy(u.UUID_128, UUID_ANCS, 16);
      svc_start = svc_end = 0;
      start_proc(aci_gatt_disc_primary_service_by_uuid(conn, UUID_TYPE_128, &u));
      break;
    }
    case S_DISC_CHAR:
      ns_handle = 0;
      start_proc(aci_gatt_disc_all_char_of_service(conn, svc_start, svc_end));
      break;
    case S_DISC_DESC:
      ns_cccd = 0;
      start_proc(aci_gatt_disc_all_char_desc(conn, ns_handle, svc_end));
      break;
    case S_SUBSCRIBE:
      start_proc(aci_gatt_write_char_value(conn, ns_cccd, 2, cccd_on));
      break;
    default:
      break;
  }
}

static void go(step_t s)
{
  set_step(s);
  tries = 0;
  do_step();
}

/* our GATT procedure finished: decide what comes next */
static void proc_done(uint8_t error_code)
{
  proc_busy = 0;
  switch (step)
  {
    case S_DISC_SVC:
      if (svc_start) { go(S_DISC_CHAR); } else { set_step(S_NOT_FOUND); }
      break;
    case S_DISC_CHAR:
      if (ns_handle) { go(S_DISC_DESC); } else { set_step(S_NOT_FOUND); }
      break;
    case S_DISC_DESC:
      if (!ns_cccd) { ns_cccd = ns_handle + 1; }   /* usual position */
      go(S_SUBSCRIBE);
      break;
    case S_SUBSCRIBE:
      if (att_error == ATT_ERR_INSUF_AUTHENTICATION || att_error == ATT_ERR_INSUF_ENCRYPTION)
      {
        SLOG(">> ancs: phone wants pairing first (ATT err 0x%02X)\n", att_error);
        aci_gap_peripheral_security_req(conn);
        set_step(S_WAIT_PAIR);
        ticks = 0;
      }
      else if (error_code || att_error)
      {
        SLOG(">> ancs: subscribe failed proc 0x%02X att 0x%02X\n", error_code, att_error);
        set_step(S_ERROR);
      }
      else
      {
        set_step(S_READY);
        SLOG(">> ancs: subscribed - iOS should now keep the link alive\n");
      }
      break;
    default:
      break;
  }
}

/* Notification Source: EventID, Flags, CategoryID, CategoryCount, NotificationUID(4) */
static void on_notification_source(const uint8_t *d, uint8_t n)
{
  static const char *const ev[] = { "added", "modified", "removed" };
  if (n < 8)
  {
    return;
  }
  SLOG(">> ancs: notification %s, category %d, count %d\n",
               (d[0] < 3) ? ev[d[0]] : "?", d[2], d[3]);
}

/* GATT / ATT events from the stack */
static SVCCTL_EvtAckStatus_t Ancs_EventHandler(void *p_Event)
{
  hci_event_pckt *p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);
  if (p_event_pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
  {
    return SVCCTL_EvtNotAck;
  }
  evt_blecore_aci *e = (evt_blecore_aci *)p_event_pckt->data;

  switch (e->ecode)
  {
    case ACI_ATT_FIND_BY_TYPE_VALUE_RESP_VSEVT_CODE:          /* service found */
    {
      aci_att_find_by_type_value_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || step != S_DISC_SVC) break;
      if (r->Num_of_Handle_Pair > 0)
      {
        svc_start = r->Attribute_Group_Handle_Pair[0].Found_Attribute_Handle;
        svc_end   = r->Attribute_Group_Handle_Pair[0].Group_End_Handle;
      }
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE:                /* characteristics */
    {
      aci_att_read_by_type_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || step != S_DISC_CHAR) break;
      /* entry: decl handle(2) properties(1) value handle(2) UUID(16) = 21 bytes */
      if (r->Handle_Value_Pair_Length == 21)
      {
        for (uint8_t i = 0; i + 21 <= r->Data_Length; i += 21)
        {
          const uint8_t *t = &r->Handle_Value_Pair_Data[i];
          if (memcmp(t + 5, UUID_NS, 16) == 0)
          {
            ns_handle = (uint16_t)(t[3] | (t[4] << 8));
          }
        }
      }
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE:                   /* descriptors */
    {
      aci_att_find_info_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || step != S_DISC_DESC) break;
      if (r->Format == 1)                                     /* handle(2) uuid16(2) */
      {
        for (uint8_t i = 0; i + 4 <= r->Event_Data_Length; i += 4)
        {
          const uint8_t *t = &r->Handle_UUID_Pair[i];
          uint16_t h = (uint16_t)(t[0] | (t[1] << 8));
          uint16_t u = (uint16_t)(t[2] | (t[3] << 8));
          if (u == 0x2902 && h > ns_handle && (!ns_cccd || h < ns_cccd))
          {
            ns_cccd = h;                                      /* first CCCD after NS */
          }
        }
      }
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_ERROR_RESP_VSEVT_CODE:
    {
      aci_gatt_error_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || !proc_busy) break;
      att_error = r->Error_Code;
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_PROC_COMPLETE_VSEVT_CODE:
    {
      aci_gatt_proc_complete_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || !proc_busy) break;
      proc_done(r->Error_Code);
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_NOTIFICATION_VSEVT_CODE:
    {
      aci_gatt_notification_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || !ns_handle || r->Attribute_Handle != ns_handle) break;
      on_notification_source(r->Attribute_Value, r->Attribute_Value_Length);
      return SVCCTL_EvtAckFlowEnable;
    }
    default:
      break;
  }
  return SVCCTL_EvtNotAck;
}

/* ---------------- public ---------------- */
void ancs_client_init(void)
{
  SVCCTL_RegisterSvcHandler(Ancs_EventHandler);
}

void ancs_client_on_connect(uint16_t c)
{
  conn = c;
  proc_busy = 0;
  retry_pending = 0;
  ticks = 0;
  set_step(S_WAIT);          /* give the phone ~1.5 s for its own setup */
}

void ancs_client_on_disconnect(void)
{
  conn = NO_CONN;
  proc_busy = 0;
  step = S_IDLE;
}

void ancs_client_on_pairing(uint8_t status)
{
  if (status == 0 && step == S_WAIT_PAIR && !proc_busy)
  {
    go(S_SUBSCRIBE);         /* paired: retry subscription now */
  }
}

void ancs_client_tick(void)
{
  if (conn == NO_CONN || proc_busy)
  {
    return;
  }
  ticks++;

  switch (step)
  {
    case S_WAIT:
      if (ticks >= 3) { go(S_DISC_SVC); }
      break;
    case S_WAIT_PAIR:
      if (ticks >= 6) { ticks = 0; go(S_SUBSCRIBE); }   /* every 3 s */
      break;
    case S_DISC_SVC:
    case S_DISC_CHAR:
    case S_DISC_DESC:
    case S_SUBSCRIBE:
      if (retry_pending) { do_step(); }
      break;
    default:
      break;
  }
}

uint8_t ancs_client_ready(void)
{
  return (step == S_READY);
}

uint8_t ancs_client_failed(void)
{
  return (step == S_NOT_FOUND || step == S_ERROR);
}
