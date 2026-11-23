# BLE_TFT_AutoLock – TFT project + Laptop AutoLock in one firmware

**Board:** NUCLEO-WBA25CE1 + 2.4" TFT shield (8-bit parallel, ILI9341-compatible)
**Project folder:** `C:\work\stm_projects\BLE_TFT_AutoLock`
**Built from:** `BLE_TFT` (display, LVGL, menus 1–9) + `BLE_autolock` (laptop auto-lock)
**Date:** 20–21 Nov 2026

Related documents:

| Document | What it covers |
|---|---|
| `TFT_SETUP.md` (this folder) | TFT wiring, display driver, LVGL, menus 1–9, BLE_TFT services |
| `TFT_WIRING_DEBUG.md` (this folder) | White-screen problem, wiring test modes, how we found D6/D7 faults |
| `C:\work\stm_projects\BLE_autolock\AUTOLOCK_SETUP.md` | How AutoLock works in detail (RSSI, EMA, ANCS, HID keyboard, settings over BLE) |
| **this file** | How the two projects were combined, what is different, how to use and debug it |

---

## 1. What it does

Everything from BLE_TFT is still there (menus 1–9). Added:

- The board watches your **iPhone's Bluetooth signal**. Walk away → it presses **Win+L** on the
  laptop (the board is a Bluetooth keyboard). Come back → it presses **Space** so the PIN box appears.
- **Menu 10 – AutoLock log:** the log that used to go to Tera Term is now **on the TFT**.
- **Menu 11 – AutoLock:** big NEAR/FAR display, signal graph, Pause and Forget-pairings buttons.

```
 iPhone ◄── BLE link (ANCS keeps it alive, RSSI measured) ──►  NUCLEO + TFT  ◄── BLE link (keyboard) ──► Windows laptop
                                                                │
                                         menu 10: log      menu 11: NEAR/FAR + graph
```

---

## 2. How the project was made

1. **Copied BLE_TFT** to `BLE_TFT_AutoLock` (without the `Debug` folder).
2. **Renamed the project:** `.project` name, `.cproject` paths/artifact, new launch file
   `BLE_TFT_AutoLock.launch` (old one renamed to `BLE_TFT.launch.old`).
3. Built → same size as BLE_TFT → copy OK.
4. White screen → **wiring fault**, not code (D6/RS touching, D7 open) → see `TFT_WIRING_DEBUG.md`.
5. **Merged the AutoLock engine** (step B) + log screen (menu 10).
6. **AutoLock screen** (step C, menu 11).
7. Fixed: cut-off text, log line wrapping, laptop "ding", disconnect handling.

---

## 3. Using it

### Menus (B1: tap = next, hold = select)

| Menu | What |
|---|---|
| 1–9 | unchanged from BLE_TFT (Hello, Buttons, Dashboard, Speed, Bluetooth, Message, Image, Swap colours, Now playing) |
| **10. AutoLock log** | status lines + newest log entries (long lines wrap) + Pause/Resume button |
| **11. AutoLock** | big NEAR/FAR box, signal graph, countdown, settings, Pause, Forget pairings |

### Screen 11

```
┌──────────────────────────────────────────┐
│ < Menu          AutoLock                 │
│ ┌──────────┐   -43.7 dBm                 │
│ │   NEAR   │   Phone:  OK                │
│ └──────────┘   Laptop: ready             │
│                LOCK in 3500 ms           │
│ ┌──────────────────────────────────────┐ │
│ │ white = signal, green = NEAR level,  │ │
│ │ red = FAR level, last 30 s           │ │
│ └──────────────────────────────────────┘ │
│ NEAR > -54   FAR < -62   wait 5 s        │
│ [ Pause ]            [ Forget pairings ] │
└──────────────────────────────────────────┘
```

| Element | Meaning |
|---|---|
| Box **NEAR** green / **FAR** red / **PAUSED** yellow / **NO PHONE** grey | current state |
| dBm | smoothed signal (`filt`) |
| `LOCK in … ms` / `NEAR in … ms` | a state change is coming |
| Graph | right edge = now, 30 s history, top −30 dBm, bottom −100 dBm |
| Pause | stops locking/waking (auto-resume after 60 min) |
| Forget pairings | 1st press → red "Sure? press again" (5 s), 2nd press → forget all, drop links |

**Reading the graph:** white above green → NEAR after 2 s; white between the lines → no change;
white below red for 5 s → FAR → Win+L. Use it to tune: if white dips below red while you sit
at the desk, lower FAR (e.g. −68); if you must walk very far, raise it (e.g. −60).

### Pairing (first time / after Forget pairings)

1. **iPhone:** Settings → Bluetooth → forget old "TFT Display" / "AutoLock" if present →
   connect once to **TFT_XX** with nRF Connect or ST BLE Toolbox → **Pair**. Close the app.
2. **Windows:** Bluetooth & devices → remove old device → **Add device → TFT Display**.
3. Menu 10 should show `link … is the PHONE`, `link … is the LAPTOP/other`; status `Laptop: keyboard ready`.

Bluetooth names: advertised **TFT_XX** (XX = address byte), device name **TFT Display**.

### Settings from the phone

Same as AutoLock (`AUTOLOCK_SETUP.md` §8): nRF Connect → service `A11C0001-…` → Config `A11C0002`
6 bytes `[near][far][far hold s][near hold s][ema][pause min]`, e.g. `CAC20502043C` = −54, −62, 5, 2, 4, 60.
Saved in flash; screen 11 shows the new values at once.

---

## 4. What is different from the stand-alone AutoLock project

| Topic | BLE_autolock (no TFT) | BLE_TFT_AutoLock | Why |
|---|---|---|---|
| Log | UART → Tera Term (COM462) | **on the TFT** (menu 10) | UART TX = PA6 = **LCD_D7** |
| LEDs blue/red NEAR/FAR | yes | **no** (screen instead) | LD1 = PA7 = LCD_D5, LD3 = PB15 = LCD_D6 |
| B1 | pause | **LVGL menu key** (tap next / hold select) | needed to drive the UI |
| B2 hold 3 s | forget pairings | **screen button** with "Sure?" | B2 = PC13 = **LCD_RS** (ghost presses!) |
| Pause | B1 | screen button (menus 10 and 11) | |
| Scan response | ANCS | ANCS (was AMS in BLE_TFT) | only one 128-bit UUID fits; ANCS keeps iPhone connected |
| Adv data | `ALock_XX` + HID + appearance | `TFT_XX` + HID UUID + appearance (ST data removed) | keyboard must be advertised |
| Now Playing (AMS) | – | follows the **phone** link only | laptop is connected too |
| Phone link drops while NEAR | FAR at once (lock) | **wait FAR hold (5 s)**, lock only if not back | short drops caused unwanted locks |
| Wake key | on every NEAR | **only after a real lock** | Space on boot/reconnect made Windows "ding" |

---

## 5. Code changes (where to look)

All AutoLock changes in ST files are marked `/* BLE_TFT_AutoLock */`.

### New / copied files

| File | Purpose |
|---|---|
| `STM32_WPAN/App/presence.c/.h` | link identification (ANCS probe → PHONE / LAPTOP), RSSI + EMA + NEAR/FAR, pause, forget, grace time, `presence_get_info()` for the screens |
| `STM32_WPAN/App/ancs_client.c/.h` | ANCS subscription = iPhone keep-alive |
| `STM32_WPAN/App/hid_keyboard.c/.h` | HID keyboard services, Win+L / Space to the laptop, phone kept away from the keyboard |
| `STM32_WPAN/App/settings.c/.h` | settings service + flash storage |
| `Core/Src/screen_log.c`, `Core/Inc/screen_log.h` | **log ring buffer** (40 lines × 60 chars), `SLOG()` printf-style |
| `Core/Src/ui_autolock.c` | menu 10 – log screen |
| `Core/Src/ui_autolock_view.c` | menu 11 – AutoLock screen |

All new `.c` files are linked in `STM32CubeIDE/.project`.

### Changed files

| File | Change |
|---|---|
| `STM32_WPAN/App/app_ble.c` | includes; `hid_keyboard_init()` + `settings_service_init()` after `ams_client_init()`; `presence_init()`; connect → `presence_on_connect()` + **restart advertising** (2nd device); disconnect → log reason + `presence_on_disconnect()`; pairing → log failures + `presence_on_pairing()`; numeric comparison answered on the **event's** link; scan response → ANCS; `Adv_Cancel` → slow advertising (never stop); adv data with HID UUID + keyboard appearance; `ble_ui_get_state()` = connected if any link; `ble_ui_conn_handle()` = **phone** link |
| `Core/Src/ams_client.c` | follows the phone link (`ble_ui_conn_handle()`), restarts if it changes |
| `Core/Inc/app_conf.h` | `CFG_BLE_NUM_GATT_SERVICES 12`, `CFG_BLE_NUM_GATT_ATTRIBUTES 100`, `CFG_BLE_ATT_VALUE_ARRAY_SIZE 2300`, appearance `0x03C1`, `CFG_TASK_RSSI_ID` |
| `STM32_WPAN/App/ble_conf.h` | `BLE_CFG_SVC_MAX_NBR_CB 10` (event handlers: p2p, text, image, ams, ancs, hid, settings) |
| `STM32CubeIDE/STM32WBA25CEUX_FLASH.ld` | FLASH `512K - 16K - 4K` (4 KB page for settings at 0x0807B000) |
| `Core/Inc/ui.h`, `Core/Src/ui_home.c` | menu entries 10 and 11 |
| `Core/Src/main.c` | `TFT_PIN_TEST` wiring/colour/ID test modes (0 = normal) |

### How the log reaches the screen

```
presence.c / hid_keyboard.c / ...   SLOG("...")      (was LOG_INFO_APP → UART)
        │
screen_log.c   ring buffer, "mm:ss text", one call may hold several lines
        │
ui_autolock.c  timer 300 ms: if slog_count() changed → slog_get(last 9) → label (wraps)
```

`SLOG()` is called from the Bluetooth code in the main loop (sequencer) – never from interrupts –
so no locking is needed. To log from your own code: `#include "screen_log.h"` and `SLOG("x=%d\n", x);`

### Memory (21 Nov 2026 build)

| | Used | Available |
|---|---|---|
| Flash | 409,464 code + 20,076 data ≈ **420 KB** | 492 KB (512 − 16 NVM − 4 settings) |
| RAM | 20,076 data + 71,056 bss ≈ **89 KB** | 96 KB |

RAM is the tight one – keep new buffers small.

---

## 6. Log reference (menu 10)

| Log line | Meaning |
|---|---|
| `presence: new link 0x0001` | a device connected |
| `ancs: WAIT … READY` | iPhone subscription steps |
| `presence: link 0x0001 is the PHONE` / `is the LAPTOP/other` | link identified |
| `hid: Report Map read by … (phone) -> DENIED` | iPhone kept from using the board as keyboard (normal) |
| `hid: link … subscribed to key reports` | laptop (or phone) switched on key reports |
| `NEAR - welcome back` / `FAR - LOCK` | state change |
| `hid: pressing Win+L`, `key 08 0F -> 0x0000 sent(00) listen YES` | lock key sent to the laptop |
| `hid: wake key (screen on)`, `key 00 2C …` | Space sent (only after a lock) |
| `presence: phone link lost - lock in 5 s …` / `phone back - no lock` | grace time |
| `disconnect 0x0001 reason 0x13` | link dropped; 0x13 = other side hung up, 0x08 = timeout (out of range), 0x16 = board hung up |
| `pairing 0x0001 status 0x.. reason 0x..` | pairing failed – see §7 |
| `ancs: notification added, category 4` | an iPhone notification arrived |

---

## 7. Troubleshooting (problems we hit)

| Symptom | Cause | Fix |
|---|---|---|
| White screen (also with old BLE_TFT) | wiring: D6/RS touching, D7 open | `TFT_WIRING_DEBUG.md`, test modes `TFT_PIN_TEST 1/2/3` |
| Ghost B2 presses, pairings erased, `pairing KO 0x03 / 0x06` | **old BLE_autolock firmware** with the shield attached (B2 = LCD_RS) | flash BLE_TFT_AutoLock; forget/remove on iPhone + Windows; pair again |
| "Phone OK Laptop: ready" cut off on screen 11 | one line too long | split into two lines |
| Log lines cut off | label in clip mode | `LV_LABEL_LONG_WRAP`, newest at bottom |
| Laptop "ding" when opening screen 11 | phone reconnect → NEAR → Space typed into an unlocked laptop | wake key only after a real lock |
| Phone link drops → unexpected lock | link loss = FAR at once | 5 s grace time before locking |
| `pairing … FAILED` repeating | iPhone still has old keys | iPhone: Forget This Device → pair again |
| Now Playing empty | AMS waits for the phone to be identified (ANCS READY) | wait a few seconds after connecting; check menu 10 |

---

## 8. Lessons learned

- **Combine by copying + merging hooks**, never by regenerating from the `.ioc`.
- Check **pin sharing** before reusing features: buttons, LEDs and UART all collide with the TFT here.
- With **two links**, every "the connection" in old code (`connectionHandle`, `ble_ui_conn_handle`)
  must become "**which** connection" (phone or laptop).
- A **log on the device's own screen** is very handy when the UART pin is taken.
- Always look at the **reason codes** of disconnects and pairing failures – they point straight at the cause.
- Don't act on single events (one link drop, one reconnect) – add a **grace time**.

---

## 9. Touch screen (21–23 Nov 2026)

Menu **12. Touch**: touch now works on every screen (B1 still works). D5/D6 wires swapped.
Full description: **TOUCH_SETUP.md**.
