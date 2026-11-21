/**
 * hid_keyboard.c - BLE_AutoLock Step 4: HID over GATT keyboard (HOGP)
 *
 * GATT services a host (Windows) expects from a BLE keyboard:
 *   HID 0x1812
 *     HID Information 0x2A4A  (read)          bcdHID 1.11, country 0, flags
 *     Report Map      0x2A4B  (read)          "I am a keyboard, 8-byte reports"
 *     HID Control Pt  0x2A4C  (write w/o rsp) suspend / exit suspend (ignored)
 *     Report (input)  0x2A4D  (read, notify)  the key presses
 *        + CCCD (auto) + Report Reference 0x2908 = {ID 0, type Input}
 *   Battery 0x180F : Battery Level 0x2A19 = 100 %
 *   Device Information 0x180A : PnP ID 0x2A50
 *
 * A key press is one 8-byte input report:
 *   [modifiers][reserved][key1..key6]   modifiers bit 3 = Left GUI (Windows key)
 *   Win+L = 08 00 0F 00 00 00 00 00     (0x0F = usage code of 'L')
 *   release = all zeros
 *
 * The iPhone is bonded to the board too. If iOS read the Report Map it would
 * treat the board as a hardware keyboard and hide the on-screen keyboard.
 * So every Report Map read waits for the app: phone -> denied, other -> allowed.
 * (presence.c knows which link is the phone: the one where ANCS was found.)
 */
#include "main.h"
#include "app_common.h"
#include "log_module.h"
#include "screen_log.h"   /* BLE_TFT_AutoLock: log goes to the TFT */
#include "ble_core.h"
#include "svc_ctl.h"
#include "presence.h"
#include "hid_keyboard.h"

#define NO_CONN   0xFFFFU

/* Standard keyboard report descriptor (same layout as the USB boot keyboard) */
static const uint8_t report_map[] =
{
  0x05, 0x01,   /* Usage Page (Generic Desktop)            */
  0x09, 0x06,   /* Usage (Keyboard)                        */
  0xA1, 0x01,   /* Collection (Application)                */
  0x05, 0x07,   /*   Usage Page (Key Codes)                */
  0x19, 0xE0,   /*   Usage Minimum (Left Ctrl)             */
  0x29, 0xE7,   /*   Usage Maximum (Right GUI)             */
  0x15, 0x00,   /*   Logical Minimum (0)                   */
  0x25, 0x01,   /*   Logical Maximum (1)                   */
  0x75, 0x01,   /*   Report Size (1)                       */
  0x95, 0x08,   /*   Report Count (8)   -> modifier bits   */
  0x81, 0x02,   /*   Input (Data, Variable, Absolute)      */
  0x95, 0x01,   /*   Report Count (1)                      */
  0x75, 0x08,   /*   Report Size (8)                       */
  0x81, 0x01,   /*   Input (Constant)   -> reserved byte   */
  0x95, 0x06,   /*   Report Count (6)                      */
  0x75, 0x08,   /*   Report Size (8)                       */
  0x15, 0x00,   /*   Logical Minimum (0)                   */
  0x25, 0x65,   /*   Logical Maximum (101)                 */
  0x05, 0x07,   /*   Usage Page (Key Codes)                */
  0x19, 0x00,   /*   Usage Minimum (0)                     */
  0x29, 0x65,   /*   Usage Maximum (101)                   */
  0x81, 0x00,   /*   Input (Data, Array) -> 6 key codes    */
  0xC0          /* End Collection                          */
};

static const uint8_t hid_info[4]   = { 0x11, 0x01, 0x00, 0x02 }; /* HID 1.11, no country, normally connectable */
static const uint8_t report_ref[2] = { 0x00, 0x01 };             /* Report ID 0, Input report */
static const uint8_t battery[1]    = { 100 };
static const uint8_t pnp_id[7]     = { 0x02, 0x83, 0x04, 0x50, 0x57, 0x00, 0x01 }; /* USB VID 0x0483 (ST), PID 0x5750, v1.0 */

static const uint8_t key_win_l[8]  = { 0x08, 0x00, 0x0F, 0, 0, 0, 0, 0 };
static const uint8_t key_shift[8]  = { 0x02, 0, 0, 0, 0, 0, 0, 0 }; /* Left Shift alone (Step 5a) */
static const uint8_t key_space[8]  = { 0x00, 0, 0x2c, 0, 0, 0, 0, 0 };
static const uint8_t key_none[8]   = { 0 };

static uint16_t hid_svc, map_h, input_h;
#define MAX_TARGETS  4
static uint16_t held[4] = { NO_CONN, NO_CONN, NO_CONN, NO_CONN }; /* Report Map reads waiting for an answer (one per link) */
static uint8_t  release_pending;

/* ---------- small helpers to add services / characteristics ---------- */
static uint16_t add_service16(uint16_t uuid, uint8_t max_records)
{
  Service_UUID_t u;
  uint16_t h = 0;
  u.Service_UUID_16 = uuid;
  tBleStatus st = aci_gatt_add_service(UUID_TYPE_16, &u, PRIMARY_SERVICE, max_records, &h);
  if (st != BLE_STATUS_SUCCESS)
  {
    SLOG(">> hid: add service 0x%04X FAILED 0x%02X\n", uuid, st);
  }
  return h;
}

static uint16_t add_char16(uint16_t svc, uint16_t uuid, uint8_t len, uint8_t props,
                           uint8_t perm, uint8_t evt_mask, const uint8_t *value)
{
  Char_UUID_t u;
  uint16_t h = 0;
  u.Char_UUID_16 = uuid;
  tBleStatus st = aci_gatt_add_char(svc, UUID_TYPE_16, &u, len, props, perm, evt_mask,
                                    0x10, CHAR_VALUE_LEN_CONSTANT, &h);
  if (st != BLE_STATUS_SUCCESS)
  {
    SLOG(">> hid: add char 0x%04X FAILED 0x%02X\n", uuid, st);
    return 0;
  }
  if (value)
  {
    aci_gatt_update_char_value(svc, h, 0, len, value);
  }
  return h;
}

/* ---------- GATT server events ---------- */
static SVCCTL_EvtAckStatus_t Hid_EventHandler(void *p_Event)
{
  hci_event_pckt *p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);
  if (p_event_pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
  {
    return SVCCTL_EvtNotAck;
  }
  evt_blecore_aci *e = (evt_blecore_aci *)p_event_pckt->data;

  switch (e->ecode)
  {
    case ACI_GATT_READ_PERMIT_REQ_VSEVT_CODE:              /* someone reads the Report Map */
    {
      aci_gatt_read_permit_req_event_rp0 *r = (void *)e->data;
      if (r->Attribute_Handle != map_h + 1) break;
      uint16_t c = r->Connection_Handle;
      presence_link_t kind = presence_link_kind(c);
      if (kind == LINK_PHONE)
      {
        aci_gatt_permit_read(c, 1, 0x08, 0);   /* deny: insufficient authorization */
        SLOG(">> hid: Report Map read by 0x%04X (phone) -> DENIED\n", c);
      }
      else if (kind == LINK_OTHER)
      {
        aci_gatt_permit_read(c, 0, 0, 0);      /* allow */
        SLOG(">> hid: Report Map read by 0x%04X (laptop) offset %d -> allowed\n", c, r->Offset);
      }
      else
      {
        /* not identified yet: answer when the ANCS probe finishes */
        for (int i = 0; i < 4; i++)
        {
          if (held[i] == NO_CONN || held[i] == c) { held[i] = c; break; }
        }
        SLOG(">> hid: Report Map read by 0x%04X (unknown) -> held\n", c);
      }
      return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE:           /* CCCD of the input report */
    {
      aci_gatt_attribute_modified_event_rp0 *r = (void *)e->data;
      if (r->Attr_Handle != input_h + 2) break;
      /* A bonded laptop writes this only once (at pairing) and expects us to
       * remember it. The stack saves it to flash only at disconnect - a reset
       * or reflash while connected loses it. So save it right now. */
      SLOG(">> hid: link 0x%04X %s key reports\n", r->Connection_Handle,
                   (r->Attr_Data_Length > 0 && (r->Attr_Data[0] & 0x01)) ? "subscribed to" : "unsubscribed from");
      aci_gatt_store_db();
      return SVCCTL_EvtAckFlowEnable;
    }
    default:
      break;
  }
  return SVCCTL_EvtNotAck;
}

/* ---------- public ---------- */
void hid_keyboard_init(void)
{
  /* HID: 1 service + 4 chars * 2 + CCCD + Report Reference = 11 records */
  hid_svc = add_service16(0x1812, 12);
  add_char16(hid_svc, 0x2A4A, sizeof(hid_info), CHAR_PROP_READ,
             ATTR_PERMISSION_ENCRY_READ, GATT_DONT_NOTIFY_EVENTS, hid_info);
  map_h = add_char16(hid_svc, 0x2A4B, sizeof(report_map), CHAR_PROP_READ,
                     ATTR_PERMISSION_ENCRY_READ, GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP, report_map);
  add_char16(hid_svc, 0x2A4C, 1, CHAR_PROP_WRITE_WITHOUT_RESP,
             ATTR_PERMISSION_ENCRY_WRITE, GATT_DONT_NOTIFY_EVENTS, NULL);
  input_h = add_char16(hid_svc, 0x2A4D, 8, CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                       ATTR_PERMISSION_ENCRY_READ | ATTR_PERMISSION_ENCRY_WRITE,
                       GATT_NOTIFY_ATTRIBUTE_WRITE, key_none);

  Char_Desc_Uuid_t d;
  uint16_t dh;
  d.Char_UUID_16 = 0x2908;                                 /* Report Reference */
  tBleStatus st = aci_gatt_add_char_desc(hid_svc, input_h, UUID_TYPE_16, &d,
                                         sizeof(report_ref), sizeof(report_ref), report_ref,
                                         ATTR_PERMISSION_ENCRY_READ, ATTR_ACCESS_READ_ONLY,
                                         GATT_DONT_NOTIFY_EVENTS, 0x10, CHAR_VALUE_LEN_CONSTANT, &dh);
  if (st != BLE_STATUS_SUCCESS)
  {
    SLOG(">> hid: add Report Reference FAILED 0x%02X\n", st);
  }

  /* Battery: 1 service + 1 char * 2 + CCCD */
  uint16_t bas = add_service16(0x180F, 4);
  add_char16(bas, 0x2A19, 1, CHAR_PROP_READ | CHAR_PROP_NOTIFY,
             ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS, battery);

  /* Device Information: PnP ID */
  uint16_t dis = add_service16(0x180A, 3);
  add_char16(dis, 0x2A50, sizeof(pnp_id), CHAR_PROP_READ,
             ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS, pnp_id);

  SVCCTL_RegisterSvcHandler(Hid_EventHandler);
  SLOG(">> hid: keyboard services added (report map handle 0x%04X, input 0x%04X)\n",
               map_h, input_h);
}

/* send one report to every connected link that is NOT the phone (= the laptop).
 * The stack only really sends it if that link has notifications switched on. */
static void send_report(const uint8_t *r)
{
  uint16_t t[MAX_TARGETS];
  uint8_t n = presence_get_others(t, MAX_TARGETS);
  for (uint8_t i = 0; i < n; i++)
  {
    /* CCCD of this link: bit 0 = the laptop switched key reports on */
    uint8_t cccd[2] = { 0, 0 };
    uint16_t len = 0, vlen = 0;
    aci_gatt_get_attribute_value(t[i], input_h + 2, 0, 2, &len, &vlen, cccd);
    tBleStatus st = aci_gatt_update_char_value_ext(t[i], hid_svc, input_h, 0x01,
                                                   8, 0, 8, r);
    SLOG("key %02X %02X -> 0x%04X %s(%02X) listen %s\n",
                 r[0], r[2], t[i], (st == BLE_STATUS_SUCCESS) ? "sent" : "FAILED", st,
                 (cccd[0] & 0x01) ? "YES" : "NO-re-pair!");
  }
}

void hid_keyboard_lock(void)
{
  if (!hid_keyboard_ready())
  {
    SLOG(">> hid: no laptop connected - cannot press Win+L\n");
    return;
  }
  SLOG(">> hid: pressing Win+L\n");
  send_report(key_win_l);
  release_pending = 1;
}

/* Step 5a: tap Shift - wakes the screen / shows the PIN box. Never unlocks. */
void hid_keyboard_wake(void)
{
  if (!hid_keyboard_ready())
  {
    return;
  }
  SLOG(">> hid: wake key (screen on)\n");
  send_report(key_space);
  release_pending = 1;
}

void hid_keyboard_tick(void)
{
  if (release_pending)
  {
    release_pending = 0;
    send_report(key_none);                     /* release all keys */
  }
}

uint8_t hid_keyboard_ready(void)
{
  uint16_t t[MAX_TARGETS];
  return presence_get_others(t, MAX_TARGETS) > 0;
}

void hid_keyboard_on_disconnect(uint16_t conn)
{
  for (int i = 0; i < 4; i++)
  {
    if (held[i] == conn) held[i] = NO_CONN;
  }
}

void hid_keyboard_link_known(uint16_t conn, uint8_t is_phone)
{
  for (int i = 0; i < 4; i++)
  {
    if (held[i] == conn)
    {
      held[i] = NO_CONN;
      aci_gatt_permit_read(conn, is_phone ? 1 : 0, is_phone ? 0x08 : 0, 0);
      SLOG(">> hid: held Report Map read by 0x%04X -> %s\n", conn,
                   is_phone ? "DENIED (phone)" : "allowed (laptop)");
    }
  }
}
