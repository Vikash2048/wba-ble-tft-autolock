/**
 * phone_time.c - Smart desk clock: read time + battery from the iPhone.
 *
 * Steps on the phone link, one GATT procedure at a time (each ends with
 * ACI_GATT_PROC_COMPLETE). We start only when ANCS (keep-alive) and AMS
 * (Now playing) have finished their own discovery - one client procedure at
 * a time per link.
 *
 *   CTS:  find service 0x1805 -> find char 0x2A2B -> read it -> notifications on
 *   BAS:  find service 0x180F -> find char 0x2A19 -> read it -> notifications on
 *   READY: notifications arrive when the phone's time is changed / battery changes.
 *          Every 30 min we read the time again (keeps our clock exact).
 *
 * Between syncs the clock runs on HAL_GetTick() (1 ms tick from the 32 MHz
 * radio crystal - drifts only a few seconds per day).
 */
#include "main.h"
#include "app_common.h"
#include "ble_core.h"
#include "svc_ctl.h"
#include "screen_log.h"
#include "presence.h"
#include "ams_client.h"
#include "phone_time.h"
#include <string.h>

#define NO_CONN          0xFFFFU
#define RESYNC_MS        (30UL * 60UL * 1000UL)
#define START_WAIT_TICKS 20        /* max 10 s waiting for AMS to finish */

typedef enum {
  S_IDLE, S_WAIT,
  S_CTS_SVC, S_CTS_CHAR, S_CTS_READ, S_CTS_SUB,
  S_BAS_SVC, S_BAS_CHAR, S_BAS_READ, S_BAS_SUB,
  S_READY
} step_t;

static step_t   step = S_IDLE;
static uint16_t conn = NO_CONN;
static uint8_t  proc_busy, att_error, retry, ticks;
static uint16_t svc_start, svc_end, found_h;
static uint16_t time_h, bat_h;            /* value handles on the phone */

/* clock */
static uint8_t  time_valid;
static int32_t  base_days;                /* days since 2000-01-01 at sync  */
static uint32_t base_sec;                 /* seconds since midnight at sync */
static uint32_t base_tick, last_sync_tick;
static int8_t   battery = -1;

/* ---------------- calendar helpers ---------------- */
static int32_t days_from_civil(int32_t y, uint32_t m, uint32_t d)  /* -> days since 2000-01-01 */
{
  y -= m <= 2;
  int32_t era = (y >= 0 ? y : y - 399) / 400;
  uint32_t yoe = (uint32_t)(y - era * 400);
  uint32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  uint32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (int32_t)doe - 730425;   /* 730425 = days 0000-03-01 .. 2000-01-01 */
}

static void civil_from_days(int32_t z, uint16_t *y, uint8_t *m, uint8_t *d)
{
  z += 730425;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  uint32_t doe = (uint32_t)(z - era * 146097);
  uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t yy = (int32_t)yoe + era * 400;
  uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  uint32_t mp = (5 * doy + 2) / 153;
  *d = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
  *m = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
  *y = (uint16_t)(yy + (*m <= 2));
}

/* Current Time 0x2A2B: year(2, LE) month day hour min sec weekday fractions adjust */
static void on_time_value(const uint8_t *v, uint8_t n)
{
  if (n < 7) return;
  uint16_t y = (uint16_t)(v[0] | (v[1] << 8));
  if (y < 2000 || v[2] < 1 || v[2] > 12 || v[3] < 1 || v[3] > 31) return;
  base_days = days_from_civil(y, v[2], v[3]);
  base_sec  = (uint32_t)v[4] * 3600U + (uint32_t)v[5] * 60U + v[6];
  base_tick = last_sync_tick = HAL_GetTick();
  if (!time_valid) SLOG("clock: time from iPhone %02u:%02u:%02u %02u.%02u.%u\n",
                        v[4], v[5], v[6], v[3], v[2], y);
  time_valid = 1;
}

/* ---------------- GATT procedures ---------------- */
static uint8_t start(tBleStatus st)
{
  if (st == BLE_STATUS_SUCCESS) { proc_busy = 1; att_error = 0; retry = 0; return 1; }
  retry = 1;                                 /* stack busy: try again next tick */
  return 0;
}

static void do_step(void)
{
  static const uint8_t on[2] = { 0x01, 0x00 };
  UUID_t u;
  switch (step)
  {
    case S_CTS_SVC: u.UUID_16 = 0x1805; svc_start = svc_end = 0;
                    start(aci_gatt_disc_primary_service_by_uuid(conn, UUID_TYPE_16, &u)); break;
    case S_BAS_SVC: u.UUID_16 = 0x180F; svc_start = svc_end = 0;
                    start(aci_gatt_disc_primary_service_by_uuid(conn, UUID_TYPE_16, &u)); break;
    case S_CTS_CHAR:
    case S_BAS_CHAR: found_h = 0;
                    start(aci_gatt_disc_all_char_of_service(conn, svc_start, svc_end)); break;
    case S_CTS_READ: start(aci_gatt_read_char_value(conn, time_h)); break;
    case S_BAS_READ: start(aci_gatt_read_char_value(conn, bat_h)); break;
    case S_CTS_SUB:  start(aci_gatt_write_char_value(conn, time_h + 1, 2, on)); break;   /* CCCD */
    case S_BAS_SUB:  start(aci_gatt_write_char_value(conn, bat_h + 1, 2, on)); break;
    default: break;
  }
}

static void go(step_t s) { step = s; do_step(); }

static void proc_done(uint8_t err)
{
  proc_busy = 0;
  uint8_t ok = !err && !att_error;
  switch (step)
  {
    case S_CTS_SVC:  if (svc_start) go(S_CTS_CHAR);
                     else { SLOG("clock: iPhone has no Current Time service\n"); go(S_BAS_SVC); } break;
    case S_CTS_CHAR: time_h = found_h; if (time_h) go(S_CTS_READ); else go(S_BAS_SVC); break;
    case S_CTS_READ: go(S_CTS_SUB); break;
    case S_CTS_SUB:  go(S_BAS_SVC); break;      /* even if notify fails: we re-read every 30 min */
    case S_BAS_SVC:  if (svc_start) go(S_BAS_CHAR); else step = S_READY; break;
    case S_BAS_CHAR: bat_h = found_h; if (bat_h) go(S_BAS_READ); else step = S_READY; break;
    case S_BAS_READ: go(S_BAS_SUB); break;
    case S_BAS_SUB:  step = S_READY;
                     SLOG("clock: ready (time %s, battery %s)\n", time_h ? "ok" : "--", bat_h ? "ok" : "--");
                     break;
    case S_READY:    break;                      /* 30-min re-read finished */
    default: break;
  }
  (void)ok;
}

/* ---------------- events from the stack ---------------- */
static SVCCTL_EvtAckStatus_t PhoneTime_EventHandler(void *p_Event)
{
  hci_event_pckt *p = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);
  if (p->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) return SVCCTL_EvtNotAck;
  evt_blecore_aci *e = (evt_blecore_aci *)p->data;

  switch (e->ecode)
  {
    case ACI_ATT_FIND_BY_TYPE_VALUE_RESP_VSEVT_CODE:
    {
      aci_att_find_by_type_value_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || !proc_busy || (step != S_CTS_SVC && step != S_BAS_SVC)) break;
      if (r->Num_of_Handle_Pair > 0)
      {
        svc_start = r->Attribute_Group_Handle_Pair[0].Found_Attribute_Handle;
        svc_end   = r->Attribute_Group_Handle_Pair[0].Group_End_Handle;
      }
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE:         /* characteristics, 16-bit UUIDs */
    {
      aci_att_read_by_type_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || !proc_busy || (step != S_CTS_CHAR && step != S_BAS_CHAR)) break;
      uint16_t want = (step == S_CTS_CHAR) ? 0x2A2B : 0x2A19;
      if (r->Handle_Value_Pair_Length == 7)            /* decl(2) props(1) value(2) uuid(2) */
      {
        for (uint8_t i = 0; i + 7 <= r->Data_Length; i += 7)
        {
          const uint8_t *t = &r->Handle_Value_Pair_Data[i];
          if ((uint16_t)(t[5] | (t[6] << 8)) == want) found_h = (uint16_t)(t[3] | (t[4] << 8));
        }
      }
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_ATT_READ_RESP_VSEVT_CODE:
    {
      aci_att_read_resp_event_rp0 *r = (void *)e->data;
      if (r->Connection_Handle != conn || !proc_busy) break;
      if (step == S_CTS_READ || (step == S_READY && time_h)) on_time_value(r->Attribute_Value, r->Event_Data_Length);
      else if (step == S_BAS_READ && r->Event_Data_Length >= 1) battery = (int8_t)r->Attribute_Value[0];
      else break;
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
      if (r->Connection_Handle != conn) break;
      if (time_h && r->Attribute_Handle == time_h)     on_time_value(r->Attribute_Value, r->Attribute_Value_Length);
      else if (bat_h && r->Attribute_Handle == bat_h && r->Attribute_Value_Length) battery = (int8_t)r->Attribute_Value[0];
      else break;
      return SVCCTL_EvtAckFlowEnable;
    }
    default: break;
  }
  return SVCCTL_EvtNotAck;
}

/* ---------------- public ---------------- */
void phone_time_init(void)
{
  SVCCTL_RegisterSvcHandler(PhoneTime_EventHandler);
}

void phone_time_tick(void)
{
  uint16_t ph = presence_phone_handle();
  if (ph != conn)                                 /* phone came, went or changed */
  {
    conn = ph;
    proc_busy = 0; retry = 0; ticks = 0;
    time_h = bat_h = 0;
    step = (conn == NO_CONN) ? S_IDLE : S_WAIT;
    if (conn == NO_CONN) battery = -1;            /* the clock itself keeps running */
    return;
  }
  if (conn == NO_CONN || proc_busy) return;

  if (step == S_WAIT)
  {
    ams_state_t a = ams_client_state();
    ticks++;
    if (a == AMS_ST_READY || a == AMS_ST_NOT_FOUND || a == AMS_ST_ERROR || ticks >= START_WAIT_TICKS)
      go(S_CTS_SVC);
    return;
  }
  if (retry) { retry = 0; do_step(); return; }

  /* re-read the time every 30 min (only when nothing else is running) */
  if (step == S_READY && time_h && HAL_GetTick() - last_sync_tick > RESYNC_MS)
  {
    last_sync_tick = HAL_GetTick();               /* don't hammer if the read fails */
    start(aci_gatt_read_char_value(conn, time_h));
  }
}

int phone_time_now(phone_datetime_t *t)
{
  if (!time_valid) return 0;
  uint32_t s = base_sec + (HAL_GetTick() - base_tick) / 1000U;
  int32_t days = base_days + (int32_t)(s / 86400U);
  s %= 86400U;
  civil_from_days(days, &t->year, &t->month, &t->day);
  t->hour = (uint8_t)(s / 3600U);
  t->minute = (uint8_t)((s / 60U) % 60U);
  t->second = (uint8_t)(s % 60U);
  t->weekday = (uint8_t)(((days + 5) % 7 + 7) % 7 + 1);   /* 2000-01-01 was a Saturday (6) */
  return 1;
}

int phone_battery(void) { return battery; }

uint32_t phone_time_synced_ago_s(void)
{
  return time_valid ? (HAL_GetTick() - last_sync_tick) / 1000U : 0xFFFFFFFFU;
}
