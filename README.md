# WBA BLE TFT AutoLock

A Bluetooth LE gadget built on the **STM32 NUCLEO-WBA25CE1** with a 320×240 parallel TFT shield and an **LVGL** user interface.

It does two jobs:

1. **Phone companion display:** show messages, pictures and iPhone "Now playing" info that arrive over BLE.
2. **Laptop auto-lock:** watch your iPhone's signal strength (RSSI). When you walk away, it locks your Windows laptop (Win+L), acting as a BLE keyboard. When you come back, it wakes the laptop.

All the logs and live status appear **on the TFT itself**, so you don't need a serial terminal.

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
| D5 | PA7 | CN4-6 |
| D6 | PB15 | CN4-38 |
| D7 | PA6 | CN3-37 |
| WR | PA5 | CN6-3 |
| RS | PC13 | CN4-30 |
| CS | GND | CN5-7 |
| RD | 3.3 V (IOREF) | CN5-2 |
| RST | 3V3 | CN3-16 |
| 3V3 / 5V / GND | power | CN5-4 / CN5-5 / CN5-6 |

B1 = PA1 (menu button), LD2 = PB12.

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
2. **iPhone:** pair from the Settings → Bluetooth menu and confirm the 6-digit code on both screens. *Now playing* and auto-lock start automatically.
3. **Laptop:** in Windows *Bluetooth & devices*, add the device. It shows up as a keyboard.
4. Open menu **11. AutoLock** and walk away. The status turns red (FAR) and the laptop locks.
5. To adjust the thresholds, write the AutoLock settings characteristic from a phone app such as nRF Connect.
6. **Forget pairings** (menu 11, press twice) clears all bonds.

---

## Project layout

```
Core/Src/          main.c, TFT driver, ui_*.c screens, screen_log.c
Core/lvgl/         LVGL library
STM32_WPAN/App/    app_ble.c, BLE services, ams_client, ancs_client,
                   presence, hid_keyboard, settings
STM32CubeIDE/      CubeIDE project files
tools/             image_sender.html (Web Bluetooth, Android Chrome)
*.md               setup and debug guides
```

## Documentation

- `TFT_SETUP.md`: display, LVGL and BLE features, step by step
- `AUTOLOCK_SETUP.md`: auto-lock concepts, daily use, troubleshooting
- `TFT_WIRING_DEBUG.md`: how to find wiring faults

## Known limitations

- RAM use is about 90 of 96 KB, so keep new features small.
- CS is tied to GND, so the panel can lose byte sync. Menu 8 fixes this. The permanent fix is to move CS to PA12.
- `image_sender.html` needs Web Bluetooth, which works in Android Chrome but not in iOS Safari.
- The MCU is an engineering sample, so temperature uses single-point calibration.

## Credits and license

- Based on STMicroelectronics' **BLE_p2pServer** example. The ST files keep their original license (see the headers and the STM32CubeWBA package).
- [LVGL](https://lvgl.io) is under the MIT license.
- Original code in this repo: MIT. See `LICENSE`.
