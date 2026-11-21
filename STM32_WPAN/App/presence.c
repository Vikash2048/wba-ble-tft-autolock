/**
 * presence.c - BLE_AutoLock: phone near/far detection + what to do about it
 *
 * Every 500 ms a timer fires. The timer callback runs in interrupt context,
 * so it only schedules a sequencer task (presence_task) that does the work.
 *
 * Step 1: keep the iPhone connected without any app (ANCS subscription,
 *         see ancs_client.c), read its RSSI.
 * Step 2: smooth RSSI with an EMA and decide NEAR/FAR with hysteresis:
 *            filt = filt + (raw - filt) / EMA_DIV        (fixed point x16)
 *            NEAR -> FAR  : filt below FAR_DBM  for FAR_HOLD_MS
 *            FAR  -> NEAR : filt above NEAR_DBM for NEAR_HOLD_MS
 *            phone disconnect : FAR immediately
 *         LEDs: blue = NEAR, red = FAR.
 * Step 3: log "*** FAR - LOCK ***" (tools/autolock.ps1 can still use it).
 * Step 5a: on NEAR tap Shift so the laptop shows the PIN box (never unlocks).
 * Step 5b: B1 = pause (presentation mode): tracking keeps running but no
 *          Win+L / Shift is sent. State LED blinks. Auto-resume after 60 min.
 * Step 5d: B2 held 3 s = forget all pairings (short press only explains).
 * Step 4: two links at once - iPhone + laptop. Every new link is probed for
 *         ANCS: found = PHONE (track its RSSI), not found = OTHER (laptop).
 *         On FAR the board presses Win+L on the laptop (hid_keyboard.c).
 */
#include "main.h"
#include "app_common.h"
#include "app_conf.h"
#include "log_module.h"
#include "screen_log.h"   /* BLE_TFT_AutoLock: log goes to the TFT */
#include "ble_core.h"
#include "stm32_rtos.h"
#include "stm32_timer.h"
#include "app_bsp.h"
#include "presence.h"
#include "ancs_client.h"
#include "hid_keyboard.h"
#include "settings.h"

#define TICK_MS          500U
#define NO_CONN          0xFFFFU
#define MAX_LINKS        4

/* ---- tuning ---- */
/* Step 5c: tuning values now come from settings.c (phone + flash) */
#define NEAR_DBM         ((int32_t)settings_get()->near_dbm)          /* above = at the desk      */
#define FAR_DBM          ((int32_t)settings_get()->far_dbm)           /* below = walked away      */
#define FAR_HOLD_MS      ((uint32_t)settings_get()->far_hold_s * 1000U)
#define NEAR_HOLD_MS     ((uint32_t)settings_get()->near_hold_s * 1000U)
#define PAUSE_MAX_MS     ((uint32_t)settings_get()->pause_min * 60000UL)
#define EMA_DIV          ((int32_t)settings_get()->ema_div)

typedef struct
{
  uint16_t        handle;
  presence_link_t kind;
  uint8_t         sec_req_pending;
} link_t;

static UTIL_TIMER_Object_t tick_timer;
static link_t   links[MAX_LINKS];
static uint16_t phone = NO_CONN;        /* the iPhone's connection           */
static uint16_t probing = NO_CONN;      /* link the ANCS client is checking  */

static presence_state_t state = PRESENCE_FAR;
static int32_t  filt_x16;
static uint8_t  filt_valid;
static uint32_t cond_ms;
static int8_t   last_raw;         /* BLE_TFT_AutoLock: for the screen */
static uint8_t  lock_sent;        /* Win+L really went to a laptop -> wake key allowed later */
static uint8_t  phone_lost;       /* phone link dropped while NEAR: wait before locking */
static uint32_t lost_ms;
static uint8_t  paused;           /* Step 5b */
static uint32_t paused_ms;
static uint8_t  blink;

/* Scan response: Service Solicitation (AD 0x15), 128-bit ANCS
 * 7905F431-B5CE-4E99-A40F-4B1E122D00D0, little-endian */
static const uint8_t scan_rsp[] =
{
  17, 0x15,
  0xD0, 0x00, 0x2D, 0x12, 0x1E, 0x4B, 0x0F, 0xA4,
  0x99, 0x4E, 0xCE, 0xB5, 0x31, 0xF4, 0x05, 0x79,
};

/* ---------------- links ---------------- */
static link_t *find_link(uint16_t h)
{
  for (int i = 0; i < MAX_LINKS; i++)
  {
    if (links[i].kind != LINK_NONE && links[i].handle == h) return &links[i];
  }
  return NULL;
}

presence_link_t presence_link_kind(uint16_t h)
{
  link_t *l = find_link(h);
  return l ? l->kind : LINK_NONE;
}

uint8_t presence_get_others(uint16_t *out, uint8_t max)
{
  uint8_t n = 0;
  for (int i = 0; i < MAX_LINKS && n < max; i++)
  {
    if (links[i].kind == LINK_OTHER) out[n++] = links[i].handle;
  }
  return n;
}

static void set_kind(link_t *l, presence_link_t kind)
{
  l->kind = kind;
  SLOG(">> presence: link 0x%04X is the %s\n", l->handle,
               (kind == LINK_PHONE) ? "PHONE" : "LAPTOP/other");
  hid_keyboard_link_known(l->handle, kind == LINK_PHONE);
}

/* ---------------- NEAR / FAR ---------------- */
static void show_state_led(void)
{
  if (state == PRESENCE_NEAR) { APP_BSP_LED_Off(LED_RED);  APP_BSP_LED_On(LED_BLUE); }
  else                        { APP_BSP_LED_Off(LED_BLUE); APP_BSP_LED_On(LED_RED);  }
}

static void set_state(presence_state_t s)
{
  if (s == state) return;
  state = s;
  show_state_led();
  if (s == PRESENCE_NEAR)
  {
    SLOG("\n*** NEAR - welcome back ***\n\n");
    /* wake key only if we locked the laptop before - otherwise Space would be typed
     * into whatever window is open (Windows "ding") on every boot / reconnect */
    if (!paused && lock_sent) hid_keyboard_wake();   /* Step 5a: PIN screen ready */
    lock_sent = 0;
  }
  else
  {
    SLOG(paused ? "\n*** FAR - (paused, not locking) ***\n\n" : "\n*** FAR - LOCK ***\n\n");
    if (!paused)
    {
      lock_sent = hid_keyboard_ready();        /* a laptop is there to receive it */
      hid_keyboard_lock();                     /* Step 4: Win+L on the laptop */
    }
  }
}

/* Step 5b: B1 pressed */
void presence_toggle_pause(void)
{
  paused = !paused;
  paused_ms = 0;
  SLOG(paused ? "\n>> PAUSED - auto-lock OFF (press B1 again to resume)\n\n"
                      : "\n>> RESUMED - auto-lock ON\n\n");
  show_state_led();
}

/* Step 5d (TFT version): B2 can't be used - PC13 is LCD_RS. Called from the screen.
 * Forget all pairings and drop every link: devices must pair again. */
void presence_forget_all(void)
{
  tBleStatus st = aci_gap_clear_security_db();
  SLOG("ALL PAIRINGS FORGOTTEN (0x%02X)\n", st);
  SLOG("now Forget on iPhone + Remove in Windows, pair again\n");
  for (int i = 0; i < MAX_LINKS; i++)
  {
    if (links[i].kind != LINK_NONE)
    {
      aci_gap_terminate(links[i].handle, 0x13);
    }
  }
}

static void pause_tick(void)
{
  if (!paused) return;
  paused_ms += TICK_MS;
  if (paused_ms >= PAUSE_MAX_MS)
  {
    SLOG(">> pause timed out after 60 min\n");
    presence_toggle_pause();
    return;
  }
  blink = !blink;                              /* blink the state LED while paused */
  if (blink) { APP_BSP_LED_Off(LED_RED); APP_BSP_LED_Off(LED_BLUE); }
  else       { show_state_led(); }
}

static void process_rssi(int8_t raw)
{
  if (!filt_valid)
  {
    filt_x16 = (int32_t)raw * 16;
    filt_valid = 1;
    cond_ms = 0;
  }
  else
  {
    filt_x16 += ((int32_t)raw * 16 - filt_x16) / EMA_DIV;
  }
  int32_t filt = filt_x16 / 16;

  if (state == PRESENCE_NEAR)
  {
    cond_ms = (filt < FAR_DBM) ? cond_ms + TICK_MS : 0;
    if (cond_ms >= FAR_HOLD_MS) { cond_ms = 0; set_state(PRESENCE_FAR); }
  }
  else
  {
    cond_ms = (filt > NEAR_DBM) ? cond_ms + TICK_MS : 0;
    if (cond_ms >= NEAR_HOLD_MS) { cond_ms = 0; set_state(PRESENCE_NEAR); }
  }

  last_raw = raw;                            /* shown on the TFT (no log line every 0.5 s) */

  settings_update_status(state == PRESENCE_NEAR, (int8_t)filt, raw, paused, hid_keyboard_ready());
}

/* ---------------- 500 ms task ---------------- */
static void tick_timer_cb(void *arg)
{
  (void)arg;
  UTIL_SEQ_SetTask(1U << CFG_TASK_RSSI_ID, CFG_SEQ_PRIO_0);   /* interrupt: only schedule */
}

static void presence_task(void)
{
  /* 1. ask each new link to pair / encrypt (one per tick) */
  for (int i = 0; i < MAX_LINKS; i++)
  {
    if (links[i].kind != LINK_NONE && links[i].sec_req_pending)
    {
      links[i].sec_req_pending = 0;
      tBleStatus s = aci_gap_peripheral_security_req(links[i].handle);
      SLOG(">> presence: security request 0x%04X %s\n", links[i].handle,
                   (s == BLE_STATUS_SUCCESS) ? "sent" : "FAILED");
      break;
    }
  }

  /* 2. identify links: probe unknown ones for ANCS, one at a time */
  if (probing == NO_CONN)
  {
    for (int i = 0; i < MAX_LINKS; i++)
    {
      if (links[i].kind == LINK_UNKNOWN)
      {
        if (phone != NO_CONN)
        {
          set_kind(&links[i], LINK_OTHER);   /* already have the phone */
        }
        else
        {
          probing = links[i].handle;
          ancs_client_on_connect(probing);
          break;
        }
      }
    }
  }
  ancs_client_tick();
  if (probing != NO_CONN)
  {
    link_t *l = find_link(probing);
    if (l && ancs_client_ready())
    {
      set_kind(l, LINK_PHONE);
      phone = probing;
      probing = NO_CONN;                     /* ANCS stays subscribed on the phone */
      filt_valid = 0;
      cond_ms = 0;
    }
    else if (l && ancs_client_failed())
    {
      set_kind(l, LINK_OTHER);
      ancs_client_on_disconnect();           /* free the ANCS client for the next link */
      probing = NO_CONN;
    }
  }

  /* 2b. phone link lost while NEAR: wait FAR_HOLD before locking */
  if (phone_lost)
  {
    if (phone != NO_CONN)
    {
      phone_lost = 0;
      SLOG(">> presence: phone back - no lock\n");
    }
    else if ((lost_ms += TICK_MS) >= FAR_HOLD_MS)
    {
      phone_lost = 0;
      SLOG(">> presence: phone did not come back\n");
      set_state(PRESENCE_FAR);               /* -> Win+L */
    }
  }

  /* 3. track the phone */
  if (phone != NO_CONN)
  {
    uint8_t raw = 0;
    if (hci_read_rssi(phone, &raw) == BLE_STATUS_SUCCESS && (int8_t)raw != 127)
    {
      process_rssi((int8_t)raw);
    }
  }

  /* 4. release the Win+L keys pressed on the previous tick */
  hid_keyboard_tick();

  /* 5. pause mode: blink + auto-resume */
  pause_tick();

  /* 6. Step 5c: save settings to flash when the phone changed them */
  settings_tick();


}

/* ---------------- public ---------------- */
void presence_init(void)
{
  UTIL_SEQ_RegTask(1U << CFG_TASK_RSSI_ID, UTIL_SEQ_RFU, presence_task);
  ancs_client_init();
  UTIL_TIMER_Create(&tick_timer, TICK_MS, UTIL_TIMER_PERIODIC, tick_timer_cb, NULL);
  UTIL_TIMER_Start(&tick_timer);
  APP_BSP_LED_On(LED_RED);                   /* start as FAR (no phone yet) */
}

presence_state_t presence_get_state(void)
{
  return state;
}

void presence_on_connect(uint16_t h)
{
  for (int i = 0; i < MAX_LINKS; i++)
  {
    if (links[i].kind == LINK_NONE)
    {
      links[i].handle = h;
      links[i].kind = LINK_UNKNOWN;
      links[i].sec_req_pending = 1;
      SLOG(">> presence: new link 0x%04X\n", h);
      return;
    }
  }
  SLOG(">> presence: too many links, 0x%04X ignored\n", h);
}

void presence_on_disconnect(uint16_t h)
{
  link_t *l = find_link(h);
  if (l) l->kind = LINK_NONE;
  hid_keyboard_on_disconnect(h);

  if (h == probing)
  {
    ancs_client_on_disconnect();
    probing = NO_CONN;
  }
  if (h == phone)
  {
    ancs_client_on_disconnect();
    phone = NO_CONN;
    cond_ms = 0;
    if (state == PRESENCE_NEAR)
    {
      /* BLE_TFT_AutoLock: a short drop (pairing hiccup, radio busy) must not lock the
       * laptop. Lock only if the phone stays away for FAR_HOLD (same wait as walking away). */
      phone_lost = 1;
      lost_ms = 0;
      SLOG(">> presence: phone link lost - lock in %lu s if it doesn't come back\n",
           (unsigned long)(FAR_HOLD_MS / 1000U));
    }
    else
    {
      SLOG(">> presence: phone disconnected\n");
    }
  }
  else
  {
    SLOG(">> presence: link 0x%04X disconnected\n", h);
  }
}

void presence_on_pairing(uint16_t h, uint8_t status)
{
  if (status == 0)
  {
    SLOG(">> presence: link 0x%04X PAIRED / bonded OK\n", h);
    if (h == probing) ancs_client_on_pairing(status);
  }
  else
  {
    SLOG(">> presence: link 0x%04X pairing FAILED status 0x%02X\n", h, status);
  }
}

void presence_set_scan_response(void)
{
  tBleStatus s = hci_le_set_scan_response_data(sizeof(scan_rsp), scan_rsp);
  if (s != BLE_STATUS_SUCCESS)
  {
    SLOG(">> presence: scan response FAILED 0x%02X\n", s);
  }
}

/* ---------------- BLE_TFT_AutoLock: for the screen and ble_ui ---------------- */
uint16_t presence_phone_handle(void)
{
  return phone;                              /* 0xFFFF until ANCS found the iPhone */
}

uint8_t presence_link_count(void)
{
  uint8_t n = 0;
  for (int i = 0; i < MAX_LINKS; i++) if (links[i].kind != LINK_NONE) n++;
  return n;
}

void presence_get_info(presence_info_t *in)
{
  uint16_t t[MAX_LINKS];
  in->phone   = (phone != NO_CONN);
  in->laptops = presence_get_others(t, MAX_LINKS);
  in->near    = (state == PRESENCE_NEAR);
  in->paused  = paused;
  in->kbd     = hid_keyboard_ready();
  in->raw     = last_raw;
  in->filt10  = (int16_t)((filt_x16 * 10) / 16);
  in->cond_ms = cond_ms;
}
