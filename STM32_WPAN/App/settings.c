/**
 * settings.c - BLE_AutoLock Step 5c
 *
 * GATT service "AutoLock Settings"  A11C0001-5A9E-4C1B-9F3A-6B2D7E8F0A01
 *   Config  A11C0002-...  read / write (encrypted), 6 bytes:
 *           [near_dbm][far_dbm][far_hold_s][near_hold_s][ema_div][pause_min]
 *           dBm values are signed bytes: -54 = 0xCA, -62 = 0xC2, -72 = 0xB8
 *   Status  A11C0003-...  read / notify, 5 bytes, every 0.5 s:
 *           [state 1=NEAR 0=FAR][filt dBm][raw dBm][paused][keyboard ready]
 *
 * A write is checked BEFORE the stack accepts it (write permit request):
 * bad values are refused with ATT error 0x80, so the phone app shows an error.
 * Good values are used at once and saved to flash.
 *
 * Flash: one 4 KB page just below ST's NVM area (bonds live there).
 * The linker script reserves it. Erase + write go through ST's Flash
 * Manager, which slots flash operations between radio events.
 */
#include "main.h"
#include "app_common.h"
#include "log_module.h"
#include "screen_log.h"   /* BLE_TFT_AutoLock: log goes to the TFT */
#include "ble_core.h"
#include "svc_ctl.h"
#include "flash_manager.h"
#include "settings.h"
#include <string.h>

/* ---- defaults (used until something valid is saved) ---- */
static const settings_t defaults = { -54, -62, 5, 2, 4, 60 };

/* ---- flash location: page right below the 16 KB NVM at the end of flash ---- */
#define NVM_SIZE        (16U * 1024U)
#define CFG_PAGE_ADDR   (FLASH_BASE + FLASH_SIZE - NVM_SIZE - FLASH_PAGE_SIZE)
#define CFG_PAGE_INDEX  ((CFG_PAGE_ADDR - FLASH_BASE) / FLASH_PAGE_SIZE)
#define CFG_MAGIC       0x314B4C41UL      /* "ALK1": old record (16 bytes, settings only)      */
#define CFG_MAGIC2      0x324B4C41UL      /* "ALK2": BLE_TFT_AutoLock, + touch calibration    */

typedef struct
{
  uint32_t   magic;
  settings_t s;          /* 6 bytes */
  uint8_t    cal_valid;  /* touch calibration present (BLE_TFT_AutoLock) */
  uint8_t    pad1;
  uint16_t   cal_rx[3];  /* raw touch values of the 3 calibration crosses */
  uint16_t   cal_ry[3];
  uint8_t    clock_style; /* desk clock layout 0..3 (0xFF in older records = default) */
  uint8_t    clock_h24;   /* desk clock 0 = 12 h, 1 = 24 h */
  uint8_t    pad[6];     /* total 32 bytes = two 128-bit flash writes */
} stored_t;

static settings_t cur;
static uint8_t    cal_valid;
static uint16_t   cal_rx[3], cal_ry[3];
static uint8_t    clk_style, clk_h24;    /* desk clock (defaults: layout 0, 12 h) */

/* ---- UUIDs (little-endian) ---- */
#define SET_UUID(last) { 0x01, 0x0A, 0x8F, 0x7E, 0x2D, 0x6B, 0x3A, 0x9F, \
                         0x1B, 0x4C, 0x9E, 0x5A, (last), 0x00, 0x1C, 0xA1 }
static const uint8_t UUID_SVC[16]    = SET_UUID(0x01);
static const uint8_t UUID_CONFIG[16] = SET_UUID(0x02);
static const uint8_t UUID_STATUS[16] = SET_UUID(0x03);

static uint16_t svc_h, config_h, status_h;

/* ---- flash save state machine ---- */
typedef enum { F_IDLE, F_ERASE, F_ERASING, F_WRITE, F_WRITING } fstate_t;
static fstate_t fstate = F_IDLE;
static volatile uint8_t fm_complete, fm_available;
static uint8_t  fm_waiting;          /* FM said BUSY: wait for "available" */
static uint8_t  save_again;
static uint32_t wbuf[8];             /* 32-bit aligned source for FM_Write (32 bytes) */

static void fm_cb(FM_FlashOp_Status_t st)
{
  if (st == FM_OPERATION_COMPLETE) fm_complete = 1;
  else                             fm_available = 1;
}
static FM_CallbackNode_t fm_node = { .Callback = fm_cb };

/* ---------------- validation ---------------- */
static uint8_t valid(const settings_t *s)
{
  return (s->near_dbm <= -20) && (s->near_dbm >= -100) &&
         (s->far_dbm  <= -20) && (s->far_dbm  >= -100) &&
         (s->near_dbm >= s->far_dbm + 4) &&           /* keep a gap: no flicker */
         (s->far_hold_s  >= 1) && (s->far_hold_s  <= 60) &&
         (s->near_hold_s >= 1) && (s->near_hold_s <= 60) &&
         (s->ema_div >= 1) && (s->ema_div <= 16) &&
         (s->pause_min >= 1) && (s->pause_min <= 240);
}

static void log_settings(const char *what)
{
  SLOG(">> settings %s: near %d, far %d dBm, far hold %d s, near hold %d s, ema /%d, pause %d min\n",
               what, cur.near_dbm, cur.far_dbm, cur.far_hold_s, cur.near_hold_s,
               cur.ema_div, cur.pause_min);
}

/* ---------------- public ---------------- */
void settings_init(void)
{
  const stored_t *f = (const stored_t *)CFG_PAGE_ADDR;
  if ((f->magic == CFG_MAGIC || f->magic == CFG_MAGIC2) && valid(&f->s))
  {
    cur = f->s;
    log_settings("loaded from flash");
    if (f->magic == CFG_MAGIC2 && f->cal_valid == 1)
    {
      cal_valid = 1;
      memcpy(cal_rx, f->cal_rx, sizeof cal_rx);
      memcpy(cal_ry, f->cal_ry, sizeof cal_ry);
      SLOG(">> settings: touch calibration loaded from flash\n");
    }
    if (f->magic == CFG_MAGIC2 && f->clock_style < 4 && f->clock_h24 <= 1)
    {
      clk_style = f->clock_style;
      clk_h24   = f->clock_h24;
    }
  }
  else
  {
    cur = defaults;
    log_settings("defaults");
  }
}

const settings_t *settings_get(void)
{
  return &cur;
}

static void request_save(void)
{
  if (fstate == F_IDLE) fstate = F_ERASE;
  else                  save_again = 1;          /* save once more when done */
}

void settings_tick(void)
{
  FM_Cmd_Status_t r;

  if (fm_waiting)
  {
    if (!fm_available) return;                   /* Flash Manager still busy */
    fm_available = 0;
    fm_waiting = 0;
  }

  switch (fstate)
  {
    case F_ERASE:
      fm_complete = 0;
      r = FM_Erase(CFG_PAGE_INDEX, 1, &fm_node);
      if (r == FM_OK)        fstate = F_ERASING;
      else if (r == FM_BUSY) fm_waiting = 1;
      else { SLOG(">> settings: flash erase ERROR\n"); fstate = F_IDLE; }
      break;

    case F_ERASING:
      if (fm_complete) fstate = F_WRITE;
      break;

    case F_WRITE:
    {
      stored_t st;
      memset(&st, 0xFF, sizeof st);
      st.magic = CFG_MAGIC2;
      st.s = cur;
      st.cal_valid = cal_valid;
      st.clock_style = clk_style;
      st.clock_h24 = clk_h24;
      memcpy(st.cal_rx, cal_rx, sizeof cal_rx);
      memcpy(st.cal_ry, cal_ry, sizeof cal_ry);
      memcpy(wbuf, &st, sizeof wbuf);
      fm_complete = 0;
      r = FM_Write(wbuf, (uint32_t *)CFG_PAGE_ADDR, 8, &fm_node);   /* size in 32-bit words */
      if (r == FM_OK)        fstate = F_WRITING;
      else if (r == FM_BUSY) fm_waiting = 1;
      else { SLOG(">> settings: flash write ERROR\n"); fstate = F_IDLE; }
      break;
    }

    case F_WRITING:
      if (fm_complete)
      {
        fstate = F_IDLE;
        const stored_t *f = (const stored_t *)CFG_PAGE_ADDR;
        SLOG(">> settings: %s\n",
                     (f->magic == CFG_MAGIC2 && memcmp(&f->s, &cur, sizeof cur) == 0)
                     ? "saved to flash" : "flash verify FAILED");
        if (save_again) { save_again = 0; fstate = F_ERASE; }
      }
      break;

    default:
      break;
  }
}

/* ---------------- GATT ---------------- */
static SVCCTL_EvtAckStatus_t Settings_EventHandler(void *p_Event)
{
  hci_event_pckt *p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);
  if (p_event_pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
  {
    return SVCCTL_EvtNotAck;
  }
  evt_blecore_aci *e = (evt_blecore_aci *)p_event_pckt->data;

  if (e->ecode == ACI_GATT_WRITE_PERMIT_REQ_VSEVT_CODE)
  {
    aci_gatt_write_permit_req_event_rp0 *r = (void *)e->data;
    if (r->Attribute_Handle != config_h + 1) return SVCCTL_EvtNotAck;

    settings_t n;
    uint8_t ok = (r->Data_Length == sizeof n);
    if (ok)
    {
      memcpy(&n, r->Data, sizeof n);
      ok = valid(&n);
    }
    if (ok)
    {
      aci_gatt_permit_write(r->Connection_Handle, r->Attribute_Handle, 0, 0,
                            r->Data_Length, r->Data);              /* accept */
      cur = n;
      log_settings("changed from phone");
      request_save();
    }
    else
    {
      aci_gatt_permit_write(r->Connection_Handle, r->Attribute_Handle, 1, 0x80,
                            r->Data_Length, r->Data);              /* refuse */
      SLOG(">> settings: write REFUSED (need 6 bytes, near >= far + 4, ranges)\n");
    }
    return SVCCTL_EvtAckFlowEnable;
  }
  return SVCCTL_EvtNotAck;
}

static uint16_t add_char128(const uint8_t *uuid, uint8_t len, uint8_t props,
                            uint8_t perm, uint8_t evt_mask)
{
  Char_UUID_t u;
  uint16_t h = 0;
  memcpy(u.Char_UUID_128, uuid, 16);
  tBleStatus st = aci_gatt_add_char(svc_h, UUID_TYPE_128, &u, len, props, perm, evt_mask,
                                    0x10, CHAR_VALUE_LEN_CONSTANT, &h);
  if (st != BLE_STATUS_SUCCESS)
  {
    SLOG(">> settings: add char FAILED 0x%02X\n", st);
  }
  return h;
}

void settings_service_init(void)
{
  settings_init();                           /* load before the value is published */
  Service_UUID_t su;
  memcpy(su.Service_UUID_128, UUID_SVC, 16);
  /* 1 service + config (2) + status (2 + CCCD) = 6 records */
  tBleStatus st = aci_gatt_add_service(UUID_TYPE_128, &su, PRIMARY_SERVICE, 7, &svc_h);
  if (st != BLE_STATUS_SUCCESS)
  {
    SLOG(">> settings: add service FAILED 0x%02X\n", st);
    return;
  }
  config_h = add_char128(UUID_CONFIG, sizeof(settings_t), CHAR_PROP_READ | CHAR_PROP_WRITE,
                         ATTR_PERMISSION_ENCRY_READ | ATTR_PERMISSION_ENCRY_WRITE,
                         GATT_NOTIFY_WRITE_REQ_AND_WAIT_FOR_APPL_RESP);
  status_h = add_char128(UUID_STATUS, 5, CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                         ATTR_PERMISSION_ENCRY_READ, GATT_DONT_NOTIFY_EVENTS);
  aci_gatt_update_char_value(svc_h, config_h, 0, sizeof(settings_t), (const uint8_t *)&cur);
  SVCCTL_RegisterSvcHandler(Settings_EventHandler);
}

void settings_update_status(uint8_t near, int8_t filt, int8_t raw, uint8_t paused, uint8_t kbd)
{
  if (!status_h) return;
  uint8_t v[5] = { near, (uint8_t)filt, (uint8_t)raw, paused, kbd };
  aci_gatt_update_char_value(svc_h, status_h, 0, sizeof v, v);   /* notifies subscribers */
}

/* ---------------- touch calibration (BLE_TFT_AutoLock) ---------------- */
void settings_save_touch_cal(const uint16_t rx[3], const uint16_t ry[3])
{
  memcpy(cal_rx, rx, sizeof cal_rx);
  memcpy(cal_ry, ry, sizeof cal_ry);
  cal_valid = 1;
  SLOG(">> settings: touch calibration saved\n");
  request_save();
}

int settings_get_touch_cal(uint16_t rx[3], uint16_t ry[3])
{
  if (!cal_valid) return 0;
  memcpy(rx, cal_rx, sizeof cal_rx);
  memcpy(ry, cal_ry, sizeof cal_ry);
  return 1;
}

/* ---------------- desk clock layout + 12/24 h ---------------- */
void settings_get_clock(uint8_t *style, uint8_t *h24)
{
  *style = clk_style;
  *h24 = clk_h24;
}

void settings_set_clock(uint8_t style, uint8_t h24)
{
  if (style == clk_style && h24 == clk_h24) return;
  clk_style = style;
  clk_h24 = h24;
  request_save();
}
