# WBA BLE TFT AutoLock

A Bluetooth LE gadget built on the **STM32 NUCLEO-WBA25CE1** with a 320×240 parallel TFT shield and an **LVGL** user interface.

It does two jobs:

1. **Phone companion display:** show messages, pictures and iPhone "Now playing" info that arrive over BLE.
2. **Laptop auto-lock:** watch your iPhone's signal strength (RSSI). When you walk away, it locks your Windows laptop (Win+L), acting as a BLE keyboard. When you come back, it wakes the laptop.

All the logs and live status appear **on the TFT itself**, so you don't need a serial terminal.
The screen is a **resistive touch screen** (calibrated once, saved in flash); button B1 works as well.

---

## Features

Each feature has its own screen. Open them from the home menu, and use **"< Menu"** to go back.

| # | Screen | What it does |
|---|---|---|
| 1 | Hello | LVGL hello world |
| 2 | Buttons | Button / input demo |
| 3 | Dashboard | Demo dashboard widgets |
| 4 | Speed | Display fill-rate test |
| 5 | Bluetooth | BLE status, advertise, notify phone, phone-controlled LED |
| 6 | Phone message | Text sent from the phone (custom Message service) |
| 7 | Phone image | RGB565 picture streamed over BLE straight into display RAM |
| 8 | Swap colours | Fixes the panel's byte-order flip |
| 9 | Now playing | iPhone media info and controls via **Apple Media Service (AMS)** |
| 10 | AutoLock log | Auto-lock status lines and the last log lines |
| 11 | AutoLock | Big NEAR / FAR / PAUSED / NO PHONE indicator, live dBm, lock countdown, 30 s RSSI chart with thresholds, Pause/Resume, Forget pairings |
| 12 | Touch | Live raw touch values, a dot under your finger, 3-point calibration (saved in flash) |

**Navigation:** tap the screen (after calibration), or use **B1**: tap = next item, hold = select.

### How auto-lock works

- The iPhone stays connected to the board through **ANCS** (Apple Notification Center Service), which keeps the link alive.
- The board reads the RSSI and smooths it with an EMA filter. **Hysteresis** (NEAR −54 dBm / FAR −62 dBm by default) stops the state from flickering.
- The laptop pairs with the board as a **BLE HID keyboard**:
  - **FAR** for long enough → board sends **Win+L**.
  - **NEAR** again → board sends a wake key (Space).
- The settings (thresholds, timings) can be changed from the phone through a custom GATT service. They are saved in flash.
- More than one laptop can be paired at the same time.

---

## Hardware

- **NUCLEO-WBA25CE1** (STM32WBA25CEU7: 512 KB flash, 96 KB RAM, 64 MHz)
- 320×240 TFT shield with an 8-bit parallel interface
- An iPhone and a Windows laptop (for auto-lock)

### Wiring (10 signals)

| TFT | MCU pin | Nucleo connector |
|---|---|---|
| D0 | PA9 | CN6-4 |
| D1 | PA10 | CN6-5 |
| D2 | PA8 | CN6-6 |
| D3 | PA11 | CN6-9 |
| D4 | PB2 | CN6-10 |
| D5 | PB15 | CN4-38 |
| D6 | PA7 | CN4-6 |
| D7 | PA6 | CN3-37 |
| WR | PA5 | CN6-3 |
| RS | PC13 | CN4-30 |
| CS | GND | CN5-7 |
| RD | 3.3 V (IOREF) | CN5-2 |
| RST | 3V3 | CN3-16 |
| 3V3 / 5V / GND | power | CN5-4 / CN5-5 / CN5-6 |

B1 = PA1 (menu button), LD2 = PB12.

> **D5/D6 are swapped compared to the original BLE_TFT wiring** (D6 must be on PA7, an ADC pin,
> for touch). The code (`tft.c`) expects this order – if the wires don't match, the screen stays white.

### Touch panel

The resistive touch film shares 4 display pins – no extra wires:

| Touch | Display pin | MCU pin | ADC |
|---|---|---|---|
| X+ | D6 | PA7 | ADC4_IN2 |
| X− | RS | PC13 | – |
| Y+ | WR | PA5 | ADC4_IN4 |
| Y− | D7 | PA6 | ADC4_IN3 |

Touch is read between screen updates; the data lines are set to a harmless value so a
stray write pulse (CS is tied to GND) can't disturb the display. Details: `TOUCH_SETUP.md`.

> PA6 is both UART TX and LCD D7, which is why all logs go to the screen.
> If you get a white screen, see `TFT_WIRING_DEBUG.md` and the `TFT_PIN_TEST` modes in `Core/Src/main.c`.

---

## BLE services

| Service | UUID | Role |
|---|---|---|
| Environmental Sensing / Temperature | 0x181A / 0x2A6E | Server: MCU temperature |
| Message | A07E0001-… (char A07E0002) | Server: text to screen 6 |
| Image | A07E0010-… (ctrl 0011, data 0012, status 0013) | Server: picture streaming |
| AutoLock settings | A11C0001-5A9E-4C1B-9F3A-6B2D7E8F0A01 (config 0002, status 0003) | Server: thresholds and timings |
| HID (keyboard) | 0x1812 | Server: Win+L / wake key to the laptop |
| Apple Media Service | 89D3502B-0F36-433A-8EF4-C502AD55F8DC | Client: Now playing |
| ANCS | Apple | Client: keeps the iPhone link alive |

Advertised name: `TFT_XX`. GAP name: **TFT Display**. Pairing uses numeric comparison (MITM, bonding).

---

## Building

1. Install **STM32CubeIDE** and the **STM32Cube_FW_WBA V1.10.0** firmware package.
2. Clone this repo and import `STM32CubeIDE/` as an existing project.
3. Set the path variable **`WBA_FW`** to your `STM32Cube_FW_WBA_V1.10.0` folder:
   *Project → Properties → Resource → Linked Resources*. Also check the include paths in *C/C++ Build → Settings*, because some of them are absolute.
4. Build and flash.

> Don't regenerate the code from the `.ioc` file. It would overwrite the custom code and can delete `Middlewares/`. LVGL lives in `Core/lvgl` for this reason.
> When you add a source file, add it as a link in `STM32CubeIDE/.project`, then refresh the project.

---

## Using it

1. Power the board. It advertises as `TFT_XX`.
   First time only: open menu **12. Touch → Calibrate** and tap the 3 red crosses.
2. **iPhone:** pair from the Settings → Bluetooth menu and confirm the 6-digit code on both screens. *Now playing* and auto-lock start automatically.
3. **Laptop:** in Windows *Bluetooth & devices*, add the device. It shows up as a keyboard.
4. Open menu **11. AutoLock** and walk away. The status turns red (FAR) and the laptop locks.
5. To adjust the thresholds, write the AutoLock settings characteristic from a phone app such as nRF Connect.
6. **Forget pairings** (menu 11, press twice) clears all bonds.

---

## Project layout

```
Core/Src/          main.c (+ TFT_PIN_TEST test modes), TFT driver, touch.c,
                   ui_*.c screens, screen_log.c
Core/lvgl/         LVGL library
STM32_WPAN/App/    app_ble.c, BLE services, ams_client, ancs_client,
                   presence, hid_keyboard, settings
STM32CubeIDE/      CubeIDE project files
tools/             image_sender.html (Web Bluetooth, Android Chrome)
*.md               setup and debug guides
```

## Documentation

- `TFT_SETUP.md`: display, LVGL and BLE features, step by step
- `BLE_TFT_AUTOLOCK.md`: how the TFT and auto-lock projects were combined, screens 10/11, log reference
- `AUTOLOCK_SETUP.md`: auto-lock concepts, daily use, troubleshooting
- `TOUCH_SETUP.md`: how the resistive touch works, pins, calibration, troubleshooting
- `TFT_WIRING_DEBUG.md`: how to find wiring faults

### Test modes

`#define TFT_PIN_TEST` near the top of `Core/Src/main.c` (0 = normal app). Output in a serial
terminal at 115200 and on LED LD2:

| Value | Test |
|---|---|
| 1 | Wiring test: stuck / touching wires + walk test (each signal 3.3 V in turn) |
| 2 | Colour test: display only, no BLE / LVGL |
| 3 | Ask the display for its ID (needs LCD_RD moved to CN4-36) |
| 4 | Touch test: both touch layers present, LD2 on while pressed |

## Known limitations

- RAM use is about 90 of 96 KB, so keep new features small.
- CS is tied to GND, so the panel can lose byte sync. Menu 8 fixes this. The permanent fix is to move CS to PA12
  (would also make touch reads fully independent of the display).
- Touch calibration uses 3 points (linear). A 5-point least-squares calibration would also correct a tilted panel.
- `image_sender.html` needs Web Bluetooth, which works in Android Chrome but not in iOS Safari.
- The MCU is an engineering sample, so temperature uses single-point calibration.

## Credits and license

- Based on STMicroelectronics' **BLE_p2pServer** example. The ST files keep their original license (see the headers and the STM32CubeWBA package).
- [LVGL](https://lvgl.io) is under the MIT license.
- Original code in this repo: MIT. See `LICENSE`.
