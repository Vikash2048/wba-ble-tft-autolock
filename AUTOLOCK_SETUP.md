# BLE AutoLock – Laptop locks itself when you walk away

> **Note (BLE_TFT_AutoLock repo):** this guide was written for the stand-alone `BLE_autolock` project
> (name `ALock_XX`, board LEDs, B1 pause, B2 hold, serial log). In this repo the name is `TFT_XX`,
> status and log are on the TFT (menus 10/11), and pause / forget pairings are screen buttons.
> See `BLE_TFT_AUTOLOCK.md` §4 for all differences.

**Board:** NUCLEO-WBA25CE1 (STM32WBA25CEU7, Cortex-M33, BLE)
**Phone:** iPhone (no app needed)
**Laptop:** Windows (no software needed)
**Project folder:** `C:\work\stm_projects\BLE_autolock`
**ST firmware package:** `C:\Users\VikashSingh\STM32Cube\Repository\STM32Cube_FW_WBA_V1.10.0`

---

## Contents

1. [What it does](#1-what-it-does)
2. [How it works – the big picture](#2-how-it-works--the-big-picture)
3. [Project setup (from ST example)](#3-project-setup-from-st-example)
4. [Step-by-step history](#4-step-by-step-history)
5. [Concepts explained](#5-concepts-explained)
6. [Source files](#6-source-files)
7. [Using it every day](#7-using-it-every-day)
8. [Settings from the phone (nRF Connect)](#8-settings-from-the-phone-nrf-connect)
9. [Serial log reference](#9-serial-log-reference)
10. [Troubleshooting – problems we hit and how we fixed them](#10-troubleshooting--problems-we-hit-and-how-we-fixed-them)
11. [Limitations](#11-limitations)
12. [BLE API cheat sheet (STM32WBA)](#12-ble-api-cheat-sheet-stm32wba)
13. [Ideas for later](#13-ideas-for-later)

---

## 1. What it does

| You | Board | Laptop |
|---|---|---|
| Sit at the desk with your iPhone | Sees the phone's signal strong → **NEAR** (blue LED) | Normal use |
| Walk away | Signal weak for 5 s (or phone out of range) → **FAR** (red LED) | Board "presses" **Win+L** → **locked** |
| Come back | Signal strong for 2 s → **NEAR** | Board presses **Space** → PIN screen appears (you still type your PIN) |

- The iPhone needs **no app** – it stays connected like an Apple Watch.
- The laptop needs **no software** – the board is a normal **Bluetooth keyboard**.
- The board only needs USB for **power**.

Extra features: **B1 = pause** (presentation mode), **B2 hold 3 s = forget pairings**,
**settings changeable from the phone** and saved in flash.

---

## 2. How it works – the big picture

```
            BLE link 1 (bonded, ANCS)                 BLE link 2 (bonded, HID keyboard)
 iPhone  ◄──────────────────────────►  NUCLEO-WBA25  ◄──────────────────────────────►  Windows laptop
           board reads RSSI every 0.5 s     │            board sends key reports
                                            │            Win+L (lock) / Space (wake)
                                            │
                              RSSI → smoothing (EMA) → NEAR / FAR decision
                                            │
                           LEDs  •  serial log (COM port)  •  Status over BLE
```

The board is a **BLE peripheral with two connections at the same time**:

1. **iPhone link** – the board is a GATT *client* of Apple's **ANCS** (notification service).
   Subscribing to ANCS makes iOS keep the link alive and reconnect automatically.
   The board measures the **RSSI** (signal strength) of this link.
2. **Laptop link** – the board is a GATT *server* offering the **HID over GATT** keyboard
   service. Windows uses it like any Bluetooth keyboard.

Every new connection is **identified**: the board looks for ANCS on it.
Found → it's the **phone**. Not found → it's the **laptop**.

---

## 3. Project setup (from ST example)

The project is a copy of ST's example
`Projects\NUCLEO-WBA25CE1\Applications\BLE\BLE_p2pServer`.

### 3.1 Fixing paths after copying

ST's project finds library files by going *up N folders*. After copying the project to
`C:\work\stm_projects\BLE_autolock`, those paths point to `C:\` → errors like
`No rule to make target 'C:/Utilities/...'`.

Fix (in `STM32CubeIDE\.project` and `.cproject`, with the project removed from the workspace):

| In `.project` | In `.cproject` (one extra `../`, build runs in `Debug`) | Replace with |
|---|---|---|
| `PARENT-6-PROJECT_LOC` | `../../../../../../../` | `WBA_FW` (firmware root) |
| `PARENT-5-PROJECT_LOC` | `../../../../../../` | `WBA_FW/Projects` |
| `PARENT-4-PROJECT_LOC` | `../../../../../` | `WBA_FW/Projects/NUCLEO-WBA25CE1` |

Path variable added to `.project`:

```xml
<variableList>
  <variable>
    <name>WBA_FW</name>
    <value>file:/C:/Users/VikashSingh/STM32Cube/Repository/STM32Cube_FW_WBA_V1.10.0</value>
  </variable>
</variableList>
```

In `.cproject` the include paths use the absolute path
`C:/Users/VikashSingh/STM32Cube/Repository/STM32Cube_FW_WBA_V1.10.0/...`.
Project name changed to `BLE_AutoLock` in `.project`.

### 3.2 Rules for this project

- **Do not** regenerate code from `BLE_p2pServer.ioc` (it would overwrite our changes).
- New `.c` files must be added as a `<link>` in `STM32CubeIDE\.project`
  (copy the entry of `app_ble.c`), then **F5** in the IDE.
- All our changes in ST files are marked with `/* BLE_AutoLock */`.

### 3.3 Logging

`CFG_LOG_SUPPORTED = 1` in `Core\Inc\app_conf.h`. Log goes to the ST-LINK virtual COM port,
**115200 baud** (on this PC: **COM462**). Open with Tera Term / PuTTY.

---

## 4. Step-by-step history

| Step | What we built | Key lesson |
|---|---|---|
| 0 | Copy BLE_p2pServer, fix paths, build, test with ST BLE Toolbox | ST projects use relative paths |
| 1a | Read RSSI every 500 ms (`presence.c`) | Timer callback → sequencer task (never call BLE in an interrupt) |
| 1b | Pairing + ANCS "solicitation" in scan response, keep advertising slowly | Not enough: iOS still dropped the link |
| 1c | Real **ANCS client** subscribes to Notification Source (`ancs_client.c`) | iOS now keeps the link with no app and reconnects by itself |
| 2 | EMA smoothing + hysteresis → NEAR/FAR, LEDs | Raw RSSI is too noisy to use directly |
| 3 | PowerShell script locks Windows when the log says `FAR - LOCK` (`tools/`) | Quick win; needed COM462 + DTR |
| 4 | Board becomes a **BLE HID keyboard**, sends Win+L; two links at once (`hid_keyboard.c`) | Identify phone vs laptop; CCCD must be saved |
| 5a | Wake screen when you come back (Space) | A key press shows the PIN box, never unlocks |
| 5b | **B1 = pause** with blinking LED, auto-resume after 60 min | |
| 5c | **Settings over BLE**, saved in flash (`settings.c`) | Flash Manager + write-permit validation |
| 5d | **B2 hold 3 s = forget pairings** (ST demo erased them on a simple press) | |

---

## 5. Concepts explained

### 5.1 RSSI

**Received Signal Strength Indicator**, in dBm, always negative.

| RSSI | Meaning (this room) |
|---|---|
| −30 … −45 | phone right next to the board |
| −50 … −63 | at the desk |
| −70 … −80 | a few metres / another room |
| below −85 | about to disconnect |

Read on the board with `hci_read_rssi(conn_handle, &rssi)` (value 127 = not available).

### 5.2 Smoothing – EMA (`filt`)

Raw RSSI jumps ±10 dB (hand over the phone, body in the way, reflections).
An **exponential moving average** lets each new reading move the value only part of the way:

```
filt = filt + (raw - filt) / EMA_DIV        (EMA_DIV = 4 → new sample counts 25 %)
```

Example with filt = −56: a single spike of −71 moves filt only to −59.8; it settles back quickly.
A real walk-away keeps raw low, so filt keeps sliding down.
Done in fixed point (`filt_x16` = filt × 16), no float needed.

### 5.3 Hysteresis + hold time (NEAR / FAR)

Two thresholds with a gap between them, plus a time the condition must last:

```
NEAR → FAR  : filt below FAR_DBM  (−62) for FAR_HOLD  (5 s)   → Win+L
FAR  → NEAR : filt above NEAR_DBM (−54) for NEAR_HOLD (2 s)   → Space
phone disconnects                                              → FAR immediately
```

Between −62 and −54 nothing changes ("dead zone") → no flicker.
The log's last number (`4500 ms`) is how long the change condition has lasted.
LOCK only happens on a **NEAR → FAR change** – the board must be NEAR (armed) first.

### 5.4 Keeping the iPhone connected – ANCS

iOS drops a BLE link when the app that opened it closes. It keeps links only for
**accessories** using a system service. We use **ANCS** (Apple Notification Center Service),
like smartwatches do:

1. Board asks to pair (security request) → iPhone shows "Pair" → bonded.
2. Board (GATT client) finds ANCS service `7905F431-B5CE-4E99-A40F-4B1E122D00D0`.
3. Finds **Notification Source** `9FBF120D-6301-42D9-8C58-25E699A21DBD` and its CCCD.
4. Writes `01 00` to the CCCD → subscribed. iOS now keeps the link and reconnects on its own.

Bonus: every iPhone notification appears in the log (`ancs: notification added, category 4`).
Advertising never stops completely (after 60 s it goes slow, 1–2.5 s) so the phone can reconnect.

### 5.5 Two links: who is the phone?

Each new connection starts as **UNKNOWN**. The board probes it for ANCS (one link at a time):

- ANCS found → **PHONE** → its RSSI is tracked.
- Not found → **LAPTOP/other** → it gets key presses.
- If the phone is already known, a new link is the laptop straight away.

Advertising is restarted after every connection so the second device can connect.
ST's stack allows **up to 8 links** (`CFG_BLE_NUM_LINK = 8`, valid 1–8; advertising counts too).

### 5.6 HID over GATT – the board is a keyboard

Services offered to the laptop:

| Service | Characteristic | Purpose |
|---|---|---|
| HID `0x1812` | HID Information `0x2A4A` | HID 1.11, flags "normally connectable" |
| | Report Map `0x2A4B` | describes "keyboard, 8-byte reports" (45 bytes) |
| | HID Control Point `0x2A4C` | suspend/exit (ignored) |
| | Report `0x2A4D` + CCCD + Report Reference `0x2908` {ID 0, Input} | the key presses |
| Battery `0x180F` | Battery Level `0x2A19` = 100 % | Windows shows it |
| Device Info `0x180A` | PnP ID `0x2A50` (VID 0x0483 ST) | |

Advertising: name `ALock_XX`, 16-bit UUID list `0x1812`, appearance **keyboard (0x03C1)**.
All HID characteristics need an **encrypted** (paired) link.

### 5.7 Keyboard reports (8 bytes)

| Byte | Meaning |
|---|---|
| 0 | modifier bits: `01` L-Ctrl, **`02` L-Shift**, `04` L-Alt, **`08` L-Win**, `10/20/40/80` right side |
| 1 | reserved (0) |
| 2–7 | up to 6 key codes |

Key codes: A=`04` … **L=`0F`** … Z=`1D`, 1…0 = `1E`…`27`, Enter `28`, Esc `29`, Backspace `2A`,
Tab `2B`, **Space `2C`**, F1…F12 `3A`…`45`, Delete `4C`, arrows → ← ↓ ↑ = `4F 50 51 52`.

```c
key_win_l = { 0x08, 0x00, 0x0F, 0,0,0,0,0 };   // Win + L  → lock
key_space = { 0x00, 0x00, 0x2C, 0,0,0,0,0 };   // Space    → wake / PIN box
key_none  = { 0 };                             // all keys released
```

A report says what is **held right now** → after each press the board sends `key_none`
on the next 0.5 s tick. Combinations: add modifier bits (Ctrl+Alt+Del = `05 00 4C ...`).
Media keys (volume) need a different "Consumer Control" report.

### 5.8 Keeping the iPhone from using the keyboard

The iPhone is bonded too. If iOS read the Report Map it would treat the board as a hardware
keyboard and **hide the on-screen keyboard**. So every Report Map read waits for the app
(`GATT_NOTIFY_READ_REQ_AND_WAIT_FOR_APPL_RESP`):

- from the **phone** → denied (`aci_gatt_permit_read(..., 1, 0x08, 0)`)
- from the **laptop** → allowed
- from an **unknown** link → held (one per link) until the ANCS probe decides

Key reports are sent **only to connected links that are not the phone**
(`presence_get_others()` → `aci_gatt_update_char_value_ext(conn, ...)`).

### 5.9 CCCD must be saved (important bug we found)

A keyboard host writes the input-report **CCCD** (`01 00` = "send me key presses") **once**,
at pairing, and expects the keyboard to remember it. ST's stack saves it to flash **only when
the link disconnects**. Reflashing/resetting while connected lost it → reports were "sent" but
the stack silently dropped them. Fix: `aci_gatt_store_db()` right after the CCCD is written.
The log shows `laptop listening: YES/NO` (read with `aci_gatt_get_attribute_value`).

### 5.10 Timer → sequencer task

```
UTIL_TIMER (500 ms, interrupt) ──► UTIL_SEQ_SetTask(CFG_TASK_RSSI_ID) ──► presence_task() in main loop
```

BLE functions must never be called from an interrupt. The timer only raises a flag;
the sequencer runs the real work from the main loop. Same pattern everywhere in ST code.

### 5.11 Settings in flash

- WBA25 flash page = **4 KB**. ST's NVM (pairing keys) = last 16 KB (`0x0807C000`).
- We reserved the page just below it (`0x0807B000`) by shortening FLASH in the linker script
  `STM32WBA25CEUX_FLASH.ld`: `LENGTH = (512K - 16K - 4K)`.
- Stored record (16 bytes): magic `"ALK1"` + 6 setting bytes + padding.
- Erase/write go through ST's **Flash Manager** (`FM_Erase`, `FM_Write`) which fits flash
  operations between radio events. `FM_BUSY` → wait for the "available" callback before retrying.

---

## 6. Source files

All in `STM32_WPAN\App\` unless noted.

| File | What it does |
|---|---|
| **presence.c / .h** | Heart of the app. 500 ms task: security requests, link identification (ANCS probe), RSSI + EMA + NEAR/FAR, LEDs, calls Win+L / Space, B1 pause, B2 hold, settings save |
| **ancs_client.c / .h** | GATT client: find ANCS on the iPhone, subscribe to Notification Source, handle "pair first", log notifications |
| **hid_keyboard.c / .h** | HID + Battery + Device Info services, Report Map permission, key reports to the laptop |
| **settings.c / .h** | Settings service (Config + Status), validation, flash load/save |
| app_ble.c | ST file, our hooks: includes, services init, connect/disconnect/pairing hooks, keep advertising, buttons B1/B2, adv data, numeric comparison per link |
| `Core\Inc\app_conf.h` | `CFG_TASK_RSSI_ID`, appearance `0x03C1`, `CFG_BLE_NUM_GATT_ATTRIBUTES 100`, `CFG_BLE_ATT_VALUE_ARRAY_SIZE 1850`, logging |
| `STM32CubeIDE\STM32WBA25CEUX_FLASH.ld` | FLASH shortened by 4 KB for settings |
| `STM32CubeIDE\.project` | links for presence.c, ancs_client.c, hid_keyboard.c, settings.c |
| `tools\autolock.ps1 / .bat` | Step 3 laptop script (no longer needed) |

Hooks in `app_ble.c`:

| Place | Call |
|---|---|
| `APP_BLE_Init_3` (before advertising) | `hid_keyboard_init()`, `settings_service_init()` |
| `APP_BLE_Init_2` | `presence_init()` |
| both connection-complete events | `presence_on_connect(handle)` + restart advertising |
| `EVT_DISCONN_COMPLETE_1` | `presence_on_disconnect(handle)` |
| pairing complete | `presence_on_pairing(handle, status)` |
| `ADV_DATA_UPDATE_1` | `presence_set_scan_response()` (ANCS solicitation) |
| `Adv_Cancel()` | stop → start **slow** advertising (never fully stop) |
| `APP_BSP_Button1Action` | `presence_toggle_pause()` |
| `APP_BSP_Button2Action` | `presence_b2_action(long_press)` |

---

## 7. Using it every day

### LEDs

| LED | Meaning |
|---|---|
| **Blue** solid | NEAR – armed |
| **Red** solid | FAR / no phone |
| Blue/red **blinking** | **paused** (B1) |
| Red/blue **alternating fast** | B2 is being held – release to cancel |
| Green short blinks | BLE radio activity (ST code) |

### Buttons

| Button | Action |
|---|---|
| **B1** press | pause / resume auto-lock (auto-resume after 60 min) |
| **B2** short press | only prints a hint |
| **B2** hold 3 s | forget all pairings + drop links |
| Reset (black) | restart; phone and laptop reconnect by themselves |

### First-time pairing

**iPhone**
1. Open ST BLE Toolbox or nRF Connect → connect to `ALock_XX`.
2. Tap **Pair**. Log: `PAIRED`, `ancs: READY`, `link 0x000X is the PHONE`.
3. Close the app – the phone stays connected from now on.

**Windows**
1. Settings → Bluetooth & devices → **Add device → Bluetooth** → `ALock_XX` / AutoLock.
2. Confirm if asked. Log: `link ... is the LAPTOP/other`, `Report Map read ... allowed`,
   `link ... subscribed to key reports`. RSSI lines end with `kbd ok`.

### Starting fresh

Hold **B2** 3 s, then on the iPhone **Forget This Device**, in Windows **Remove device**,
and pair both again.

---

## 8. Settings from the phone (nRF Connect)

Service **AutoLock Settings** `A11C0001-5A9E-4C1B-9F3A-6B2D7E8F0A01`.

### Config `A11C0002-...` (read / write, 6 bytes)

| Byte | Setting | Default | Range |
|---|---|---|---|
| 0 | NEAR threshold, dBm (signed) | −54 (`CA`) | −100…−20, must be ≥ FAR + 4 |
| 1 | FAR threshold, dBm (signed) | −62 (`C2`) | −100…−20 |
| 2 | FAR hold, seconds | 5 | 1…60 |
| 3 | NEAR hold, seconds | 2 | 1…60 |
| 4 | smoothing divider (EMA) | 4 | 1…16 |
| 5 | pause auto-resume, minutes | 60 (`3C`) | 1…240 |

Negative dBm → hex = 256 + value: −54 `CA`, −62 `C2`, −64 `C0`, −70 `BA`, −72 `B8`, −80 `B0`.

Example: write ByteArray `C0B80502043C` → NEAR −64, FAR −72, 5 s, 2 s, /4, 60 min.
Invalid values are **refused** (ATT error 0x80). Valid values apply at once and are saved
to flash (log: `changed from phone`, `saved to flash`; after power-on: `loaded from flash`).

### Status `A11C0003-...` (read / notify, 5 bytes, every 0.5 s)

`[NEAR=1/FAR=0] [filt dBm] [raw dBm] [paused] [keyboard ready]`
Example `01-C6-C4-00-01` = NEAR, filt −58, raw −60, not paused, keyboard ready.

---

## 9. Serial log reference

```
>> settings loaded from flash: near -54, far -62 dBm, far hold 5 s, ...
>> hid: keyboard services added (report map handle 0x00xx, input 0x00xx)
>> presence: new link 0x0001
>> presence: security request 0x0001 sent
>> ancs: WAIT → DISC_SVC → DISC_CHAR → DISC_DESC → SUBSCRIBE → READY
>> presence: link 0x0001 is the PHONE
>> presence: link 0x0002 is the LAPTOP/other
>> hid: Report Map read by 0x0002 (laptop) offset 0 -> allowed
>> hid: link 0x0002 subscribed to key reports
RSSI raw  -58  filt -60.1  NEAR  0 ms  kbd ok
RSSI raw  -71  filt -63.0  NEAR  500 ms  kbd ok        ← change pending for 500 ms
*** FAR - LOCK ***
>> hid: pressing Win+L
>> hid: report 08 0F -> link 0x0002 sent (0x00), laptop listening: YES
>> hid: report 00 00 -> link 0x0002 sent (0x00), laptop listening: YES
*** NEAR - welcome back ***
>> hid: report 00 2C -> link 0x0002 sent ...           ← Space
>> PAUSED - auto-lock OFF  /  >> RESUMED - auto-lock ON
>> ancs: notification added, category 4, count 1
```

ANCS categories: 0 other, 1 incoming call, 2 missed call, 3 voicemail, 4 social, 5 schedule,
6 email, 7 news, 8 health/fitness, 9 business, 10 location, 11 entertainment.

---

## 10. Troubleshooting – problems we hit and how we fixed them

| Symptom | Cause | Fix |
|---|---|---|
| `No rule to make target 'C:/Utilities/...'` | relative paths after copying ST example | §3.1 path table (`PARENT-6`, then `PARENT-5` for `Common/WPAN`) |
| Which device in ST BLE Toolbox? | – | name `ALock_XX` (was `p2pS_XX`); strongest RSSI; disappears on reset |
| Phone disconnects when the app is closed | iOS drops app-owned links | ANCS subscription (step 1c) |
| Phone does not reconnect by itself | solicitation + bonding alone not enough; advertising stopped after 60 s | ANCS subscribe + never stop advertising (slow mode) |
| Never locks although walking away | thresholds so tight the board never became NEAR (armed) | NEAR must be reachable at the desk; check `NEAR` in log first |
| Lock range too far | FAR −80 needs a long walk | raise FAR (e.g. −72); keep NEAR ≥ FAR + 4…8 |
| Script: `listening on COM463` but nothing | wrong ST-LINK port picked; DTR off | default `-Port COM462`, `DtrEnable = true`, `-ShowLog` to see the log |
| Tera Term + script at the same time | COM port is exclusive | close Tera Term, use `autolock.bat -ShowLog` |
| `hid: no laptop connected` but Windows says Connected | code waited for a CCCD write that a bonded Windows never repeats | send to all connected non-phone links |
| Reports `sent (0x00)` but laptop does nothing | CCCD lost (reflash while connected), only one held read | `aci_gatt_store_db()` on CCCD write, held reads per link, **remove + re-pair** in Windows |
| Key presses could go to the phone | last CCCD writer won | never send to the PHONE link |
| Shift alone did not wake the screen | modifier-only report | use Space (`0x2C`) |
| B2 erased all pairings | ST demo action | B2 now needs a 3 s hold |
| iPhone misbehaves after firmware change | iOS cached the old GATT table | iPhone: Forget This Device, pair again |
| nRF Connect write error | value refused by validation | 6 bytes, NEAR ≥ FAR + 4, ranges §8; see `write REFUSED` in log |

General checks: log at 115200 on COM462; `kbd ok` on RSSI lines; `laptop listening: YES` when locking.

---

## 11. Limitations

- **Phone left on the desk** → board still sees NEAR → laptop does not lock.
  Bluetooth tracks the phone, not you.
- RSSI depends on the room, phone position (pocket/hand), bodies, walls → tune per desk.
- The board never **unlocks** (on purpose – security). You type your PIN.
- Pairing uses numeric comparison auto-confirmed by the board (no display) – fine for a hobby
  device, not for high security.
- ST BLE Toolbox no longer shows the P2P LED demo (manufacturer data replaced by keyboard data).

---

## 12. BLE API cheat sheet (STM32WBA)

| Need | Call |
|---|---|
| RSSI of a link | `hci_read_rssi(conn, &rssi)` |
| Ask peer to pair | `aci_gap_peripheral_security_req(conn)` |
| Scan response data | `hci_le_set_scan_response_data(len, data)` |
| Add service / char / descriptor | `aci_gatt_add_service`, `aci_gatt_add_char`, `aci_gatt_add_char_desc` |
| Set value (notifies all subscribers) | `aci_gatt_update_char_value(svc, char, 0, len, val)` |
| Notify one link | `aci_gatt_update_char_value_ext(conn, svc, char, 0x01, len, 0, len, val)` |
| Read a per-link value (CCCD) | `aci_gatt_get_attribute_value(conn, handle, 0, max, &len, &vlen, buf)` |
| Answer a held read / write | `aci_gatt_permit_read(conn, 0/1, err, 0)`, `aci_gatt_permit_write(conn, attr, 0/1, err, len, data)` |
| Save CCCDs of bonded links now | `aci_gatt_store_db()` |
| GATT client discovery | `aci_gatt_disc_primary_service_by_uuid`, `aci_gatt_disc_all_char_of_service`, `aci_gatt_disc_all_char_desc` |
| Write to peer (e.g. CCCD) | `aci_gatt_write_char_value(conn, handle, len, val)` (no `write_char_desc` on WBA) |
| Disconnect / forget bonds | `aci_gap_terminate(conn, 0x13)`, `aci_gap_clear_security_db()` |
| Flash | `FM_Erase(page, n, &node)`, `FM_Write(src32, dest, words, &node)` |

Handles: characteristic declaration = `h`, value = `h + 1`, CCCD = `h + 2`.
Event handlers: `SVCCTL_RegisterSvcHandler()` (max 7, `BLE_CFG_SVC_MAX_NBR_CB`), return
`SVCCTL_EvtAckFlowEnable` when the event was yours. New characteristics need room in
`CFG_BLE_NUM_GATT_ATTRIBUTES` and `CFG_BLE_ATT_VALUE_ARRAY_SIZE` (else add_char fails).

---

## 13. Ideas for later

- Low-power mode + battery (coin cell / LiPo) so it runs without USB.
- Detect "phone left on desk" (no motion) – would need a motion sensor on the phone side.
- Lock several laptops (already works: every connected non-phone link gets Win+L).
- Small companion web page / Shortcuts action to change settings without nRF Connect.
- Status page in the TFT project (BLE_TFT) reusing this code.
