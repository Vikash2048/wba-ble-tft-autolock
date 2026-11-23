# TFT Display + LVGL on NUCLEO-WBA25CE1 – Project Notes

**Status (2 Nov 2026):** This copy belongs to the **BLE_TFT** project (display + Bluetooth). **Display and Bluetooth working** (tested with ST BLE Toolbox); LVGL v9.6.0 user interface on a jumper-wired 320×240 parallel TFT, controlled with button B1. Home menu with 5 features: Hello LVGL, Button demo, Dashboard (chip temperature + chart), Speed test, Bluetooth (step 4).

| Item | Details |
|---|---|
| Dev board | ST **NUCLEO-WBA25CE1** = MCU RF board **MB2293B** (STM32WBA25CEU7, engineering sample "ES") on base board **MB1801D** |
| MCU | STM32WBA25CEU7, Cortex-M33, UFQFPN48, 512 KB flash, 96 KB RAM, **64 MHz max** (HCLK) |
| Display | SIMKEIM "tftlcd for arduino uno (spiflash)" shield, 2.4"/2.8", ILI9341-class controller |
| Display interface | **8-bit parallel, Intel 8080 (i80) bus** – not SPI. The shield's SD-card slot and flash chip use SPI (not used) |
| Graphics library | **LVGL v9.6.0** in `Core/lvgl` |
| Tools | STM32CubeMX (peripherals, clocks, code generation) + STM32CubeIDE 2.x (build / debug) |
| Project folder | `C:\work\stm_projects\tft_screen_impl` |
| Reference | Board user manual **UM3610 Rev 2** – Table 12 (Arduino pinout), Table 13 (ST morpho pinout) |

---

## 1. Timeline – what we did

| # | Date | Step | Result |
|---|---|---|---|
| 1 | 29 Oct | Identified hardware: parallel i80 display, normally 13 MCU pins | – |
| 2 | 29 Oct | Project created in **STM32CubeMX** (Board Selector → NUCLEO-WBA25CE1 → Toolchain STM32CubeIDE). CubeIDE 2.x has no built-in CubeMX | ✓ |
| 3 | 29 Oct | Debugger "No device found on target" → MB2293B RF board re-seated | ✓ |
| 4 | 29 Oct | Stacking the shield on the Arduino headers **cannot work**: the 48-pin WBA25 has only 26 GPIOs; A0–A5 and most of D0–D9 are not connected | ✗ → jumper wires |
| 5 | 29 Oct | **Pin finder** program (`pin_finder.c`) + user manual → real header positions found | ✓ |
| 6 | 29 Oct | Display reduced to **10 signal wires** (CS → GND, RD → 3.3 V, RST → 3.3 V) | ✓ |
| 7 | 29 Oct | Own minimal driver `tft.c` → **colour test works** | ✓ |
| 8 | 30 Oct | **LVGL v9.6.0** added; HardFault fixed (stack 1 KB → 8 KB); orientation (MADCTL 0xE8) and pixel byte order (low byte first) fixed | ✓ |
| 9 | 30 Oct | **Step 1:** button B1 as LVGL input + home menu + Button demo | ✓ |
| 10 | 30 Oct | **Step 2:** ADC4 via CubeMX → Dashboard with chip temperature, supply voltage, 60 s chart. CubeMX deleted `Middlewares/` → LVGL moved to `Core/lvgl`. ES chip missing 130 °C calibration → single-point calculation. Own font with ° sign | ✓ |
| 11 | 30 Oct | **Step 3:** Speed-test screen + fast driver: full-screen LVGL frame 190 → 86 ms (5 → 11.6 fps), raw fill 137 → 13 ms. Found: 64 MHz is the WBA25 maximum | ✓ |
| 12 | 31 Oct | **Step 3b:** instruction cache (ICACHE) enabled in CubeMX + `Core/lvgl` compiled with `-O2` (per-folder setting) | ✓ |
| 13 | 2 Nov | **Step 4:** new project **BLE_TFT** (copy of ST BLE_p2pServer) with all our code + Bluetooth screen; moved to `C:\work\stm_projects\BLE_TFT` | ✓ working on hardware (phone LED ↔ TFT, notifications) |
| 14 | 3 Nov | **Step 6:** picture from an Android phone over BLE (Image service + screen **7. Phone image** + Web Bluetooth page `tools/image_sender.html`), see 13.11 | built (396 KB flash), to test on hardware |
| 15 | 3 Nov | **Step 7:** iPhone "Now playing" via Apple Media Service (board = GATT client), menu **9. Now playing** with ⏮ ⏯ ⏭, see 13.12 | built (404 KB), to test with iPhone |

---

## 2. How to use the app

The board starts on the **Menu**. Everything is controlled with button **B1**:

| Action on B1 | Effect |
|---|---|
| **Tap** (< 0.5 s) | Move the highlight (focus outline) to the next item |
| **Hold** (≥ 0.5 s) | Click / toggle the highlighted item |

| Menu entry | Screen | What it shows / does |
|---|---|---|
| 1. Hello LVGL | `ui_hello.c` | Title, animated progress bar (0 → 100 → 0), uptime counter |
| 2. Button demo | `ui_buttons.c` | Push button + counter, switch → green LED **LD2**, checkbox, status line |
| 3. Dashboard | `ui_dashboard.c` | Chip temperature (°C), supply voltage, min/max, 60 s chart, diagnostics line |
| 4. Speed test | `ui_speed.c` | CPU clock + I-cache state; measures full-screen redraw (LVGL and raw) with normal and fast driver; **Fast** switch |

Every screen has a **"< Menu"** button (top-left) to go back.

---

## 3. Final wiring (TFT shield → board)

Connect with **USB unplugged**. Board orientation: USB connectors at the bottom.

### Signal pins

| # | TFT shield pin | Function | STM32 pin | Board connector & pin | Where on the board |
|---|---|---|---|---|---|
| 1 | LCD_D0 | Data bit 0 | **PA9** | **CN6 pin 4** | Right upper inner header, 7th hole from top |
| 2 | LCD_D1 | Data bit 1 | **PA10** | **CN6 pin 5** | Right upper inner header, 6th hole from top |
| 3 | LCD_D2 | Data bit 2 | **PA8** | **CN6 pin 6** | Right upper inner header, 5th hole from top |
| 4 | LCD_D3 | Data bit 3 | **PA11** | **CN6 pin 9** | Right upper inner header, 2nd hole from top |
| 5 | LCD_D4 | Data bit 4 | **PB2** | **CN6 pin 10** | Right upper inner header, 1st hole from top |
| 6 | LCD_D5 | Data bit 5 | **PA7** (also LED LD1) | **CN4 pin 6** | Right outer pin row |
| 7 | LCD_D6 | Data bit 6 | **PB15** (also LED LD3) | **CN4 pin 38** | Right outer pin row |
| 8 | LCD_D7 | Data bit 7 | **PA6** | **CN3 pin 37** | Left outer pin row |
| 9 | LCD_WR | Write strobe | **PA5** | **CN6 pin 3** | Right upper inner header, 8th hole from top |
| 10 | LCD_RS | Command (0) / Data (1) | **PC13** (also button B2) | **CN4 pin 30** | Right outer pin row |

### Tied pins and power

| TFT shield pin | Connect to | Board connector & pin | Why |
|---|---|---|---|
| LCD_CS | GND | **CN5 pin 7** | Display always selected |
| LCD_RD | 3.3 V (IOREF) | **CN5 pin 2** | We never read from the display |
| LCD_RST | 3.3 V | **CN3 pin 16** | Reset done in software (command 0x01) |
| 3V3 | 3.3 V | **CN5 pin 4** | Shield logic power |
| 5V | 5 V | **CN5 pin 5** | Backlight / shield power |
| GND | GND | **CN5 pin 6** | Ground |
| SD_SS, SD_DI, SD_DO, SD_SCK | not connected | – | SD card not used |

### Board header reference (USB at the bottom)

| Header | Position | Pins (top → bottom) |
|---|---|---|
| **CN5** (8 holes) | Left, upper inner | 1 NC, 2 IOREF (3.3 V), 3 NRST, 4 3V3, 5 5V, 6 GND, 7 GND, 8 VIN |
| **CN7** (6 holes) | Left, lower inner | A0 … A5 (not connected to MCU by default) |
| **CN6** (10 holes) | Right, upper inner | pin 10 at top → pin 1 at bottom (D15 … D8) |
| **CN8** (8 holes) | Right, lower inner | pin 8 at top → pin 1 at bottom (D7 … D0) |
| **CN3** | Left outer long pin row (ST morpho) | numbers printed beside the pins |
| **CN4** | Right outer long pin row (ST morpho) | numbers printed beside the pins |

> **Caution:** the outer rows (CN3/CN4) also carry 5V, 3V3, VIN and GND. Connect exactly to the numbered pin; never slide a wire along them while powered.

---

## 4. Pin budget – why only 10 wires

STM32WBA25CEU7 GPIOs (26 total): PA1, PA2, PA5–PA15, PB2–PB6, PB8, PB9, PB12, PB15, PC13–PC15, PH3.

| Already used by the board | Pins |
|---|---|
| Debugger (SWD) | PA13, PA14 |
| On-board QSPI flash | PA2, PA15, PB3, PB4, PB5, PB6 |
| USB | PB8, PB9 |
| ST-LINK virtual COM (VCP) RX | PA12 |
| 32 kHz crystal, BOOT0 | PC14, PC15, PH3 |

Used for the display: PA5, PA6, PA7, PA8, PA9, PA10, PA11, PB2, PB15, PC13.
Used by the app: **PA1** = button B1 (LVGL input, CN4 pin 36 / CN3 pin 30), **PB12** = LED LD2 (Button demo).
Internal only (no pins): ADC4 temperature sensor + VREFINT.
**No free GPIO is left** on this 48-pin chip (except by giving up the QSPI flash, USB or VCP RX).

**Side effects of this wiring**
- LEDs LD1 (PA7) and LD3 (PB15) may flicker while the screen draws.
- Do not press **B2** while the display runs (shares PC13 with LCD_RS).
- **PA6 is the VCP TX line**, so UART printf to the PC no longer works.

---

## 5. Pin-finder results (measured on the real board)

| Header pin | Result | Note |
|---|---|---|
| CN6 pin 3 | PA5 | ✓ |
| CN6 pin 2 (D9) | nothing | Readme says PA6 – not connected on this board |
| CN3 pin 37 | PA6 | ✓ |
| CN4 pin 30 | PC13 | Readme said CN4 pin 36 |
| CN4 pin 6 | PA7 | Detect with 3V3 (LED pin rests low) |
| CN4 pin 38 | PB15 | Detect with 3V3 (LED pin rests low) |
| CN4 pin 36 | PA1 | ✓ |
| CN8 pin 4 / pin 7 | nothing | Readme says PB12 / PA7 – need solder bridges |
| CN7 holes 1–6, CN8 holes 1–8 | nothing | Arduino nets not connected by default |

**Lesson:** on this board, the "Arduino" positions often need solder bridges; the ST morpho rows (CN3/CN4) are the reliable place to reach MCU pins. Trust the measured results over example readmes.

---

---

## 6. Software overview

### 6.1 Files

| File | Purpose |
|---|---|
| `Core/Src/main.c` | CubeMX-generated init + our code in `USER CODE` blocks; `APP_MODE` switch |
| `Core/Src/tft.c`, `Core/Inc/tft.h` | Our display driver (no library, ST HAL only): normal + fast pixel output |
| `Core/Src/lvgl_port.c`, `.h` | Connects LVGL to the display (buffer, flush, tick, dark theme) |
| `Core/Src/input_port.c`, `.h` | Button B1 as an LVGL keypad (tap = NEXT, hold = ENTER) |
| `Core/Src/sensors.c`, `.h` | ADC4: chip temperature + supply voltage every second, 60 s history |
| `Core/Src/ui.c`, `Core/Inc/ui.h` | UI helpers: new screen, back button, screen switching |
| `Core/Src/ui_home.c` | Menu – list `MENU[]`; new features are added here |
| `Core/Src/ui_hello.c`, `ui_buttons.c`, `ui_dashboard.c`, `ui_speed.c` | One file per feature screen |
| `Core/Src/lv_font_montserrat_28_deg.c` | Our generated font (ASCII + `°`) |
| `Core/Inc/lv_conf.h` | LVGL configuration |
| `Core/lvgl/` | LVGL v9.6.0 library source |
| `Core/Src/pin_finder.c`, `.h` | Pin-finder tool (only used with `APP_MODE = APP_PIN_FINDER`) |
| `TFT_SETUP.md` | This document |

All our code is either in separate files or inside CubeMX `USER CODE` blocks, so **"Generate Code" in CubeMX keeps it**.

### 6.2 Start-up sequence (`main.c`)

```
HAL_Init → SystemClock_Config (64 MHz) → MX_GPIO_Init → MX_ADC4_Init → MX_ICACHE_Init
→ BSP LEDs, buttons, COM port (CubeMX board support)
→ USER CODE: tft_init → lvgl_port_init → input_port_init → sensors_init → ui_create (menu)
→ loop: lv_timer_handler(); HAL_Delay(5);
```

### 6.3 Mode switch (`main.c`, USER CODE PD)

```c
#define APP_MODE  APP_LVGL   /* APP_PIN_FINDER (shield removed), APP_COLOR_TEST or APP_LVGL */
```

---

## 7. Display driver (`tft.c` / `tft.h`)

### 7.1 API

```c
void tft_init(void);                                      /* pins + display init */
void tft_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void tft_fill_screen(uint16_t color);
void tft_push_pixels(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                     const uint16_t *px);                 /* used by LVGL */
void tft_set_fast(int on);  int tft_get_fast(void);       /* 1 = fast (default) */
/* TFT_WIDTH 320, TFT_HEIGHT 240, TFT_MADCTL 0xE8 (landscape)
   colours RGB565: BLACK WHITE RED GREEN BLUE YELLOW CYAN MAGENTA */
```

### 7.2 How the i80 bus works
For every byte: set **RS** (0 = command, 1 = data) → put the byte on **D0–D7** → pulse **WR** low → high (the display latches on the rising edge). CS is tied to GND (always selected), RD to 3.3 V (we never read).

- **Init (MIPI DCS commands):** `0x01` software reset → `0x11` sleep out → `0x3A 0x55` 16-bit RGB565 → `0x36 0xE8` orientation → `0x29` display on.
- **Drawing:** `0x2A` column range, `0x2B` row range, `0x2C`, then 2 bytes per pixel.
- **Lookup tables** (`lutA`/`lutB`, 2 KB RAM): for every byte value the ready-made `BSRR` word for GPIOA and GPIOB → one byte = 2 register writes + 1 for WR.
- **RS on PC13:** low-speed pin shared with button B2 → `short_delay()` after every RS change. Do not press B2 while the display runs.

### 7.3 Normal and fast mode
| | Normal (`write8` per byte) | Fast (`push_fast` / `fill_fast`) |
|---|---|---|
| Code | function call per byte, 8 NOP setup + 2 NOP hold | `__attribute__((optimize("O2")))`, no call per byte, GPIO addresses in registers, `BYTE_OUT` macro |
| Waits | generous | 3 NOP setup, 1 NOP hold (ILI9341 write cycle ≥ 66 ns; 1 cycle = 15.6 ns) |
| Fill | per-byte table look-up | 4 register values computed once (~14 instructions per pixel) |
| Full-screen fill | 137 ms (103 ms with cache) | **13–16 ms** |

Fast is the default (`fast_mode = 1`). The **Fast** switch on the Speed-test screen changes it at run time.

### 7.4 Display quirks of this panel
- **Orientation:** `TFT_MADCTL 0xE8` (0x28 was upside down). Mirrored → try `0xA8` / `0x68`.
- **Pixel byte order:** the panel wants the **low byte of each RGB565 pixel first** (found with a 6-stripe colour test: red, green, blue, white, black, navy were shown as blue, magenta, green, white, dark, light). Command parameters stay high byte first.
- Black looks dark blue on this TN panel; contrast depends on viewing angle → UI uses black background, bright colours, 20–28 px fonts.

---

## 8. LVGL

### 8.1 Integration

| Item | Details |
|---|---|
| Version / location | **LVGL v9.6.0** in **`Core/lvgl/`** (`src/`, `include/`, `lvgl.h`). Left out (not needed, C++/asm): `src/libs/thorvg`, `src/libs/gltf`, `src/debugging/vg_lite_tvg`, `src/draw/sw/blend/helium`, `*.cpp`. `src/drivers` **is** needed (headers used by `lv_init.c`) |
| v9.6 note | Public headers moved to `include/lvgl/` |
| Config | `Core/Inc/lv_conf.h` |
| Build settings (`.cproject`) | Include path `../Core/lvgl`; define `LV_CONF_INCLUDE_SIMPLE`; Debug `-Og`; **folder `Core/lvgl` → `-O2`** (step 3b) |
| Display | `lv_display_create(320, 240)`, RGB565, **partial render** buffer 320 × 24 lines (15 KB) |
| Flush | `flush_cb` → `tft_push_pixels()` → `lv_display_flush_ready()` |
| Tick | `lv_tick_set_cb(HAL_GetTick)` |
| Theme | default theme, **dark**, Montserrat 20 |
| Input | keypad from B1, default group – all buttons/switches/checkboxes join automatically |
| Main loop | `lv_timer_handler(); HAL_Delay(5);` |

### 8.2 `lv_conf.h` changes from the template
| Setting | Value | Why |
|---|---|---|
| `LV_MEM_SIZE` | 40 KB | LVGL heap, fits in 96 KB RAM |
| `LV_COLOR_FORMAT_DEFAULT` | RGB565 | Matches the display |
| `LV_DEF_REFR_PERIOD` | 20 ms | Up to ~50 Hz (limited by the bus) |
| Fonts | Montserrat 14, 20, 28 (+ own 28 px with `°`) | Small / normal / title |
| Draw formats disabled | RGB888, XRGB8888, ARGB8888 premultiplied, L8, AL88, I1 | Save flash |
| Widgets disabled | calendar, keyboard, menu, msgbox, tabview, tileview, win, spinbox, imagebutton, roller, table, dropdown, list, arclabel, animimg, canvas, textarea, span | Save flash/RAM |
| `LV_BUILD_EXAMPLES`, `LV_BUILD_DEMOS` | 0 | Not needed |

### 8.3 UI structure
- `ui_new_screen()` creates a black screen; `ui_add_back_button()` adds "< Menu"; `ui_load()` shows the screen and **deletes the old one**.
- `ui_go()` switches screens **asynchronously** (`lv_async_call`) – switching directly inside a button event would delete the button while LVGL still uses it.
- Screens with timers delete them on `LV_EVENT_DELETE` (otherwise a timer would update a deleted label → crash).
- New feature = new `ui_xxx.c` + one line in `MENU[]` (`ui_home.c`) + declaration in `ui.h`.

### 8.4 Own font with the degree sign
LVGL's built-in fonts have only ASCII + symbols. Generated with LVGL's converter:

```
npx lv_font_conv@1.5.3 --font Montserrat-Medium.ttf --size 28 --bpp 4 -r 0x20-0x7F,0xB0 \
    --format lvgl --no-compress --lv-include lvgl.h --lv-font-name lv_font_montserrat_28_deg \
    -o lv_font_montserrat_28_deg.c
```

TTF from github.com/JulietaUla/Montserrat. Use with `LV_FONT_DECLARE(lv_font_montserrat_28_deg);`, write `°` as UTF-8 `"\xC2\xB0"`. More characters: add them to `-r`.

---

## 9. Feature details

### 9.1 Step 1 – button input (B1, PA1)
`input_port.c` polls `BSP_PB_GetState(B1)` (pressed = low, B1 interrupt disabled) every LVGL input read (~30 ms): tap → `LV_KEY_NEXT`, hold ≥ 500 ms → `LV_KEY_ENTER` (sent once per hold), presses < 30 ms ignored (bounce). Button demo: switch drives **LD2 (PB12, active low)**; LED off when leaving the screen.

### 9.2 Step 2 – ADC4 setup
1. **CubeMX:** Analog → ADC4 → tick *Temperature Sensor Channel* and *Vrefint Channel* → Generate Code (adds HAL ADC files, `MX_ADC4_Init()`, ADC clock = HCLK in `stm32wbaxx_hal_msp.c`).
2. **`sensors_init()`** re-initialises ADC4: HCLK/4, 12-bit, 2 ranks (1 = VREFINT, 2 = temperature), discontinuous (each `HAL_ADC_Start()` converts the next rank), sampling 814.5 cycles (sensor needs a long sample), calibration.
3. **Every second** (`lv_timer`): VDDA from VREFINT, temperature, smoothing, min/max, 60-value ring buffer (× 10). Runs in the background on all screens.
4. **Dashboard:** labels refresh every 0.5 s, chart redrawn on each new sample with Y range = data ± 1 °C. Diagnostics line: `raw`, `cal30`, `cal130`, mode `1-pt cal` / `2-pt`.

### 9.3 Step 2 – the missing calibration on our ES chip
`cal30` (TS_CAL1) = 1021 is valid, `cal130` (TS_CAL2) = **65535 (blank)**. The two-point formula then divided by 64514 instead of ~400 → stuck at 30.0 °C. `sensors_cal_ok()` checks the values; if blank, **single-point** calculation with typical slope **2.5 mV/°C** (`TS_AVG_SLOPE_UV_PER_C`). Accuracy ±2–3 °C; `TEMP_SMOOTHING` 0.3 removes 1-LSB jitter (≈ 0.3 °C).

### 9.4 How the temperature measurement works (explained)

#### 1. The temperature sensor
The STM32 has a small circuit inside the chip whose **output voltage rises in a straight line with temperature**, about **2.5 mV per °C**:

```
voltage
  │                      ╱
  │                   ╱      slope ≈ 2.5 mV per °C
0.75 V ─ ─ ─ ─ ─ ─ ●   ← at 30 °C (measured in the factory)
  │            ╱
  └──────────┼──────────── temperature
            30 °C
```

Knowing the voltage at **one known temperature** and the **change per °C** gives any temperature from the voltage.
It measures the **silicon die**, not the room – a running chip is usually a few degrees warmer than the air.

#### 2. The ADC
ADC4 converts a voltage into a number. It is **12-bit → 0 … 4095**:

```
ADC number = input voltage / VDDA × 4095
```

**VDDA (the supply voltage) is the ADC's ruler.** The same voltage gives a different number when VDDA changes, so VDDA must be known first.

#### 3. Measuring VDDA with VREFINT
The chip has an internal reference voltage **VREFINT** (~1.2 V, stable). ST measured it in the factory with exactly **3.0 V** supply and stored the result as **VREFINT_CAL**. Measuring it again now:

```
VDDA = VREFINT_CAL × 3000 mV / raw_vrefint
```

If today's reading is smaller than the factory one, the supply is higher than 3.0 V. Our board: **3.30 V**.

#### 4. The values on the dashboard

| On screen | What it is | Our chip |
|---|---|---|
| `raw` | ADC number from the temperature sensor now (ruler = 3.3 V) | ≈ 933 |
| `cal30` | Factory ADC number at **30 °C**, ruler **3.0 V** (TS_CAL1) | 1021 |
| `cal130` | Factory ADC number at **130 °C** (TS_CAL2) | 65535 = **blank** (ES chip) |
| `1-pt cal` | Only one valid factory point → typical slope used | ✓ |
| `2-pt` | Both factory points valid (normal production chips) | – |

**Why is raw (933) lower than cal30 (1021) although the chip is warmer than 30 °C?** Different rulers: raw is measured with 3.3 V, cal30 with 3.0 V. A bigger ruler gives a smaller number for the same voltage – so both are **converted to millivolts** before comparing.

#### 5. Single-point calculation ("1-pt cal") – worked example

```
Step 1  sensor voltage now:      V_now = raw   × VDDA    / 4095 = 933  × 3300 / 4095 = 751.9 mV
Step 2  sensor voltage at 30 °C: V_30  = cal30 × 3000 mV / 4095 = 1021 × 3000 / 4095 = 748.0 mV
Step 3  difference:              751.9 − 748.0 = 3.9 mV
        3.9 mV ÷ 2.5 mV per °C = 1.6 °C
        temperature = 30 + 1.6 = 31.6 °C
```

One factory point (30 °C) + typical slope (2.5 mV/°C, `TS_AVG_SLOPE_UV_PER_C` in `sensors.c`).

#### 6. Two-point calculation ("2-pt") – normal production chips
With the 130 °C point present the slope is not assumed, it comes from this exact chip:

```
ts = raw × VDDA / 3000                 (reading converted to the 3.0 V ruler)
T  = 30 + (ts − cal30) × (130 − 30) / (cal130 − cal30)
```

On our ES chip `cal130` = 65535 (erased memory), so the formula divided by 64514 instead of ~400 and the result stayed at 30.0 °C. The code now checks the values and picks 2-pt or 1-pt automatically.

**Accuracy:** 2-point ≈ ±1–2 °C; 1-point (ours) ≈ ±2–3 °C (each chip's real slope differs a little). Changes up/down are shown correctly. To fine-tune, compare with a thermometer and adjust `TS_AVG_SLOPE_UV_PER_C`.

#### 7. How the code runs
1. **Startup – `sensors_init()`:** ADC4 set to read 2 channels in order, one per start: rank 1 = VREFINT, rank 2 = temperature sensor. Sampling time **814.5 clock cycles** (the sensor output is weak and needs time to charge the ADC input). ADC self-calibration is run.
2. **Every second (LVGL timer):**
   1. read VREFINT → VDDA
   2. read temperature sensor → °C (2-pt or 1-pt)
   3. **smoothing:** new = old + 0.3 × (measured − old) (1 ADC step ≈ 0.3 °C would otherwise flicker)
   4. update min / max, store in the **60-value history** (last 60 s)
3. **Dashboard – `ui_dashboard.c`:** reads the values every 0.5 s, updates the labels, redraws the chart when a new sample arrived. Sampling continues in the background on other screens.

```
temperature sensor ─► ADC4 (raw) ─┐
                                   ├─► convert both to mV ─► compare with factory 30 °C point
VREFINT ─► ADC4 ─► VDDA ──────────┘         ─► ÷ 2.5 mV/°C ─► + 30 ─► smoothing ─► screen + chart
```

### 9.5 Step 3 – speed test, fast driver, instruction cache
- **64 MHz is the maximum** HCLK of the STM32WBA25 (CubeMX shows "64 MHz max") → speed must come from better code and the cache.
- **Speed test** (`ui_speed.c`): (a) LVGL – change screen colour + `lv_refr_now()` 5× (render 320×240 in 24-line strips + send); (b) raw – `tft_fill_screen()` 5× (send only). Each with normal and fast driver, timed with `HAL_GetTick()`.
- **Why normal was slow:** 137 ms for 153 600 bytes ≈ **57 CPU cycles per byte**.

| Setup | LVGL frame normal | LVGL frame fast | Raw fill normal | Raw fill fast |
|---|---|---|---|---|
| 64 MHz, cache off | 190–196 ms (5.0–5.2 fps) | 91–94 ms (10.5–10.9 fps) | 137 ms | 16 ms |
| 64 MHz, cache on | 157–158 ms (6.3 fps) | 86–87 ms (11.4–11.6 fps) | 103 ms | **13 ms** |
| cache on + LVGL `-O2` (3b) | _to be measured_ | _to be measured_ | _to be measured_ | _to be measured_ |

- Fast driver: sending **8–10× faster**; full LVGL frame **~2× faster**. Remaining LVGL time ≈ 15 ms sending + ~70 ms software rendering (before 3b). Normal UI updates redraw only changed areas, so they are much quicker than a full frame.
- The cache helps slow code most (many instruction fetches); the fast loop is limited mainly by the GPIO bus.

### 9.6 Step 3b – instruction cache + LVGL `-O2`
- **ICACHE:** CubeMX → System Core → ICACHE → Mode "1-way (direct mapped cache)" → Generate Code → `MX_ICACHE_Init()`.
- **Per-folder optimisation:** CubeIDE → right-click `Core/lvgl` → Properties → C/C++ Build → Settings (Configuration: Debug) → MCU/MPU GCC Compiler → Optimization → **-O2**. Stored in `.cproject` as a separate `folderInfo` with `resourcePath="Core/lvgl"`. Our own code stays `-Og` (easy debugging).

---

## 10. CubeMX rules (lessons learned)

1. **Never put own code or libraries in `Middlewares/`** – CubeMX deleted the whole folder (and LVGL with it) on "Generate Code". Put them in **`Core/`**.
2. CubeMX rewrites the list of source folders in `.cproject`, but keeps include paths, defines and optimisation settings.
3. Own code in `main.c` only inside `USER CODE BEGIN/END` blocks.
4. Linker stack size is also set in the `.ioc` (Project Manager → Linker Settings → Minimum Stack Size = **0x2000**), otherwise regenerating resets it.
5. **After every "Generate Code" check:** `Core/lvgl` exists, include path `../Core/lvgl`, `Core/lvgl` still shows its own (-O2) settings, `_Min_Stack_Size = 0x2000` in both `.ld` files. Then **F5 → Project → Clean → Build**.

---

## 11. Build and run

1. STM32CubeIDE → select project → **F5** (refresh).
2. **Project → Clean** after CubeMX changes, then **Build** (hammer) → `0 errors`.
3. **Run** (green ▶). For Live Expressions use **Debug** (bug icon) + **F8** (resume).

Approximate size: ~300 KB of 512 KB flash, ~60 KB static RAM + 8 KB stack (-Og build; LVGL at -O2 a little larger).

## 12. Troubleshooting

| Symptom | Check |
|---|---|
| No backlight | 5V and GND wires |
| White screen only | WR (CN6-3), RS (CN4-30), CS → GND, RD and RST on 3.3 V. **Full procedure + test modes: see TFT_WIRING_DEBUG.md** (20 Nov 2026: D6↔RS touching, D7 wire open) |
| Wrong / noisy colours | D0–D7 wiring (swapped or loose); pixel byte order (low byte first on this panel) |
| Noise only in fast mode | Fast timing too tight → `tft_set_fast(0)` or add a NOP in `BYTE_OUT` |
| Garbled picture | Increase `short_delay()` loop in `tft.c` |
| Image upside down / mirrored | `TFT_MADCTL` in `tft.h` (0xE8 correct here; try 0x28 / 0xA8 / 0x68) |
| HardFault right after LVGL starts | Stack too small: `_Min_Stack_Size` must be ≥ 0x2000 |
| Build errors about `lvgl.h` after CubeMX | `Core/lvgl` missing or include path `../Core/lvgl` lost |
| Temperature stuck at one value | Look at the dashboard diagnostics line (`raw`, `cal30`, `cal130`) |
| Debugger: "No device found on target" | Re-seat MB2293B, ST-LINK USB-C, JP1 on 5V_STLK, Connect under reset, update ST-LINK firmware |
| Colours sometimes right, sometimes wrong (blue ↔ yellow-green, yellow ↔ blue, black ↔ violet, blurred text) | The panel pairs the 2 bytes of each pixel the wrong way round. Likely cause: CS is tied to GND, so the panel never re-syncs its byte counting; one stray WR pulse shifts it by one byte. Command 0xF6 (ILI9341 ENDIAN) has no effect on this panel. **Fix (3 Nov 2026, confirmed): menu "8. Swap colours"** flips the byte order in the driver (`tft_set_swap`). Also GPIO speed HIGH → MEDIUM and 5/2 NOPs setup/hold in the fast path. Permanent fix if it keeps flipping: CS on a GPIO (PA12, VCP RX is unused) |

## 13. Step 4 – Bluetooth (BLE) – project `BLE_TFT` (2 Nov 2026)

### 13.1 Where
| Item | Details |
|---|---|
| Project | **`C:\work\stm_projects\BLE_TFT`** (moved here on 2 Nov 2026 from the STM32Cube repository) |
| Based on | ST example **BLE_p2pServer** (copied; Binary/EWARM/MDK-ARM left out), IDE project renamed **BLE_TFT** |
| Import | CubeIDE → File → Import → General → Existing Projects into Workspace → select the **`BLE_TFT\STM32CubeIDE`** folder (not the top folder) |
| Display-only project | `C:\work\stm_projects\tft_screen_impl` stays unchanged as a backup / reference |

**Where ST's code comes from:** ST's drivers, Bluetooth stack, utilities and common modules are **not copied** into the project; they are used directly from the firmware package
`C:\Users\VikashSingh\STM32Cube\Repository\STM32Cube_FW_WBA_V1.10.0`:
- `STM32CubeIDE/.project` – linked files use the path variable **`WBA_FW`** (defined at the end of `.project`, also visible in CubeIDE → Project → Properties → Resource → Linked Resources → Path Variables).
- `STM32CubeIDE/.cproject` – include and library paths use the full path `C:/Users/VikashSingh/STM32Cube/Repository/STM32Cube_FW_WBA_V1.10.0/...`.
- If the firmware package moves or is updated to a new version, change `WBA_FW` and the paths in `.cproject` (search & replace).
- Two include folders listed by ST (`link_layer/ll_cmd_lib/porting`, `.../shrd_utils/inc`) do not exist in the package – also in ST's original example; harmless.
- **Do not "Generate Code" from `BLE_p2pServer.ioc`** in this project: CubeMX would overwrite our changes in ST's files and the paths.

### 13.2 What was changed in ST's example
| File | Change | Why |
|---|---|---|
| `Core/Inc/app_conf.h` | `CFG_LED_SUPPORTED 0`, `CFG_BUTTON_SUPPORTED 0`, `CFG_LOG_SUPPORTED 0` | LD1/LD3 are TFT data lines, B2 (PC13) is LCD_RS, USART1 TX (PA6) is LCD_D7 |
| `STM32CubeIDE/STM32WBA25CEUX_FLASH.ld` | `_Min_Stack_Size` 0x1000 → **0x2000** | LVGL needs more stack (HardFault otherwise) |
| `STM32CubeIDE/.project` | links to our `Core/Src/*.c` files, folder link `Middlewares/lvgl` → `Core/lvgl`, `adc_ctrl.c` → our local copy | add our code to the build |
| `STM32CubeIDE/.cproject` | include path `../../Core/lvgl`, define `LV_CONF_INCLUDE_SIMPLE`, **Debug optimisation `-Og`** (was none = `-O0`) | `-O0` does not fit in flash (141 KB too big) |
| `Core/Src/main.c` (USER CODE) | after `MX_APPE_Init()`: `tft_init → lvgl_port_init → input_port_init → sensors_init → ui_create`; in the loop after `MX_APPE_Process()`: `lv_timer_handler()` | our app runs next to ST's task scheduler |
| `STM32_WPAN/App/app_ble.c` (USER CODE) | `ble_ui_get_state()`, `ble_ui_start_advertising()` | for our Bluetooth screen |
| `STM32_WPAN/App/p2p_server_app.c` (USER CODE) | `ble_ui_led()`, `ble_ui_led_writes()`, `ble_ui_notify_enabled()`, `ble_ui_send_notification()` | for our Bluetooth screen |
| `Core/Src/adc_ctrl.c` | **local copy** of `Projects/Common/WPAN/Modules/adc_ctrl.c` + `ADCCTRL_LastRaw[]` / `ADCCTRL_LastVddMv`, and typical-parameter formula when TS_CAL2 is blank | ADC4 is shared with the Bluetooth stack; we need raw values for our 0.1 °C maths. ST's shared original is untouched (BLE_KnockLock not affected) |

### 13.3 Our code in BLE_TFT
- Same files as `tft_screen_impl` (tft, lvgl_port, input_port, sensors, ui_*, font, lv_conf) + **`ble_ui.h`** + **`ui_ble.c`** (menu entry **5. Bluetooth**).
- `sensors.c` chooses automatically: with `adc_ctrl.h` present (BLE_TFT) it reads through ST's **ADC manager** (own handle, same settings ST uses for the radio); otherwise (tft_screen_impl) it configures ADC4 itself with the HAL.
- `input_port.c` / `ui_buttons.c` now set up B1 (PA1) and LD2 (PB12) themselves (ST's board-support button/LED code is switched off in BLE_TFT).
- RAM: LVGL heap **28 KB** (was 40), draw buffer **16 lines / 10 KB** (was 24 / 15 KB) – the Bluetooth stack needs ~25 KB RAM.
- **CPU clock 32 MHz** (HSE crystal, as in all ST BLE examples for this chip) → drawing about half as fast as the 64 MHz display-only project.

### 13.4 Size at step 4 (CubeIDE Debug build: 471 KB flash) – see 13.9 for the reduction to 400 KB
| Optimisation | Flash (496 KB available) | Static RAM (96 KB, 8 KB stack) |
|---|---|---|
| `-O0` | 141 KB too big ✗ | – |
| **`-Og` (Debug setting)** | **≈ 471 KB** ✓ | ≈ 74 KB + 8 KB stack |
| `-Os` (Release) | ≈ 438 KB ✓ | ≈ 74 KB + 8 KB stack |

LVGL at `-O2` (step 3b) is not set in BLE_TFT – it would probably not fit.

### 13.5 Device name (step 5.1, 2 Nov 2026)
| Name | Where (`app_ble.c`) | Old | New |
|---|---|---|---|
| Advertising name – what the phone sees when scanning | `a_AdvData[]` (AD type *Complete Local Name*) | `p2pS_XX` | **`TFT_XX`** |
| Device name – readable after connecting (GAP service) | `a_GapDeviceName[]` | `Peer to Peer Server` | **`TFT Display`** |

- `XX` is replaced at start-up by `fill_advData()` with the last byte of the board's Bluetooth address (hex), so two boards get different names.
- An advertising packet holds at most **31 bytes**: 3 flags (added by the stack) + 8 name (length, type, 6 characters) + 16 manufacturer data (ST's ID, used by ST BLE Toolbox) = 27.
- `a_AdvData[]` now gets its size from the content (the code uses `sizeof` everywhere).
- `ble_ui_device_name()` returns the name for the Bluetooth screen ("Name: TFT_3A").
- These two arrays are generated by CubeMX (outside USER CODE) – fine because we don't regenerate this project.

### 13.6 Temperature to the phone (step 5.2, 2 Nov 2026)
A **standard** Bluetooth service, so any app understands it without knowing our board:

| GATT entry | UUID | Details |
|---|---|---|
| Service | **0x181A** Environmental Sensing | primary service, 4 attributes |
| Characteristic | **0x2A6E** Temperature | **Read + Notify**, 2 bytes, **sint16 in 0.01 °C**, little endian (31.62 °C → 3162 → bytes `5A 0C`) |
| Descriptor | 0x2902 CCCD | the phone writes it to switch notifications on/off (added by the stack because of *Notify*) |

- **Bluetooth terms:** a *service* groups related data; a *characteristic* is one value inside it with properties (Read = the phone asks, Notify = the board pushes changes).
- **Code:** `Core/Src/ble_temp_service.c` / `Core/Inc/ble_temp_service.h`
  - `ble_temp_service_init()` – `aci_gatt_add_service()` + `aci_gatt_add_char()`; called in `app_ble.c` (USER CODE `APP_BLE_Init_3`, right after ST's P2P service).
  - `ble_temp_service_update(t)` – `aci_gatt_update_char_value()`; the stack stores the value and sends a notification if the phone enabled it.
  - `sensors_on_new_sample(t)` – hook called by `sensors.c` after every sample (1 s). `sensors.c` has an empty **weak** default, so `tft_screen_impl` (no Bluetooth) is unaffected; BLE_TFT overrides it.
- **Room in ST's config:** `CFG_BLE_NUM_GATT_SERVICES` 8 (now 4 used: GAP, GATT, P2P, ESS), `CFG_BLE_NUM_GATT_ATTRIBUTES` 68.
- **Bluetooth screen:** "Temperature to phone: 31.6 C (n updates)".
- **Phone:** in **nRF Connect** (or ST BLE Toolbox's service/characteristic view) → connect to TFT_xx → *Environmental Sensing* → *Temperature* → tap the notify (↓↓↓) icon → value updates every second, shown as °C.
- Size: +~0.5 KB flash.

### 13.7 Text from the phone on the TFT (step 5.3, 2 Nov 2026)
Our **own** service (no standard exists for "text message"), with self-chosen 128-bit UUIDs:

| GATT entry | UUID | Details |
|---|---|---|
| Service | `A07E0001-5A1E-4C0D-9E2B-7F3C1D0E2A10` | primary, 3 attributes |
| Characteristic | `A07E0002-5A1E-4C0D-9E2B-7F3C1D0E2A10` | **Write**, Write without response, Read; variable length 1–100 bytes (UTF-8 text) |

- UUIDs are stored **least significant byte first** in the code (that is how the BLE stack expects 128-bit UUIDs).
- **Receiving a write:** `aci_gatt_add_char(... GATT_NOTIFY_ATTRIBUTE_WRITE ...)` asks the stack to report writes → event `ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE` → ST's service controller calls every handler registered with `SVCCTL_RegisterSvcHandler()` (max 7, `BLE_CFG_SVC_MAX_NBR_CB`) → our `Text_EventHandler()` checks `Attr_Handle == char handle + 1` (value attribute), copies the text, returns `SVCCTL_EvtAckFlowEnable`. Handles of other services are left alone (`SVCCTL_EvtNotAck`).
- The handler runs in ST's sequencer (main loop) = same thread as LVGL → the screen reads the buffer directly.
- **Code:** `Core/Src/ble_text_service.c` / `.h` (service + handler), `Core/Src/ui_message.c` (menu **6. Phone message**: big wrapped text in a framed box, "n message(s), last s ago"). Created in `app_ble.c` USER CODE `APP_BLE_Init_3`.
- Fonts contain ASCII only → other characters (é, emoji …) show as boxes.
- A single BLE write carries max **MTU − 3** bytes: 20 with the default MTU 23; phones normally negotiate a bigger MTU (ST config allows 251), so 100 characters fit in one write.
- **Phone (nRF Connect):** connect TFT_xx → *Unknown Service A07E0001…* → characteristic *A07E0002…* → **↑ (write)** → choose **TEXT (UTF-8)** → type → *Send*. Board: menu **6. Phone message**.
- Size: +~1 KB flash → **≈ 473 KB of 496 KB** (≈ 23 KB left).

### 13.8 64 MHz CPU clock (step 5.4, 3 Nov 2026)
ST's BLE examples for this chip mostly run at 32 MHz (SYSCLK = HSE crystal). ST's **BLE_TransparentMode_Ux_CDC** example shows the 64 MHz setup with Bluetooth; we copied its `SystemClock_Config()` (only that function differs; system clock manager `scm_init()`, radio clocks and low-power settings were already identical).

| Setting (`main.c`, `SystemClock_Config`) | Before (32 MHz) | Now (64 MHz) |
|---|---|---|
| PLL1 | off | on, source HSE: 32 MHz ÷ M 2 = 16 MHz × N 24 = 384 MHz ÷ R 6 = **64 MHz** (P = 48 MHz, Q = 64 MHz unused) |
| SYSCLK source | HSE | **PLL** |
| AHB5 divider (radio bus) | ÷1 | **÷2** → stays **32 MHz** (radio bus limit) |
| Flash wait states | 0 | **1** (required above 32 MHz) |
| SysTick clock | LSE | HCLK ÷ 8 |

- The changed lines are marked `BLE_TFT 5.4` (generated code – fine because we don't regenerate this project).
- ST's clock manager still switches to HSE temporarily for radio calibration (`linklayer_plat.c`, `scm_setsystemclock`).
- Check: menu **4. Speed test** shows **CPU 64 MHz**; drawing ≈ 2× faster than at 32 MHz. (Still a bit slower than `tft_screen_impl`: LVGL here is `-Og`, not `-O2`, and the draw buffer is smaller.)
- To go back to 32 MHz: undo the 5 marked lines.

### 13.9 Freeing flash (step 5.5, 3 Nov 2026)
**Measured first** (linker map file of the `-Og` build, 473 KB used of 496 KB):

| Part | Flash |
|---|---|
| ST BLE stack + link layer (.a libraries) | 124 KB (fixed) |
| LVGL core | 84 KB |
| **LVGL built-in fonts 14 / 20 / 28** | **73 KB** (28 px alone 36 KB – it carries ~60 icons) |
| LVGL drawing engine | 58 KB |
| **LVGL widgets** | **36 KB** (scale 7, chart 6.5, button matrix 4.7, arc 4, slider 2.9 …) |
| ST system code | 28 KB |
| our 28 px ° font | 14 KB |
| our application code | 11 KB |
| HAL drivers | 9 KB |

**Changes (no screen lost):**
1. **One 28 px font instead of two:** `Core/Src/lv_font_montserrat_28_deg.c` regenerated with Montserrat ASCII + `°` + **only the Bluetooth icon** (FontAwesome U+F293):
   `npx lv_font_conv@1.5.3 --font Montserrat-Medium.ttf -r 0x20-0x7F,0xB0 --font FontAwesome5-Solid+Brands+Regular.woff -r 0xF293 --size 28 --bpp 4 --format lvgl --no-compress --lv-include lvgl.h --lv-font-name lv_font_montserrat_28_deg -o lv_font_montserrat_28_deg.c`
   (icon font from the LVGL repo, tag v9.2.2, `scripts/built_in_font/`). All screens use it (`LV_FONT_DECLARE` in `ui.h`); `LV_FONT_MONTSERRAT_28 0`.
   Need another icon at 28 px? Add its code to the second `-r` (codes in `lv_symbol_def.h`).
2. **Unused widgets off** in `lv_conf.h`: arc, scale, slider, spinner, button matrix, line (the default theme pulls in every *enabled* widget).
3. **Unused pixel formats off:** RGB565 swapped, RGB565A8 (display uses plain RGB565; ARGB8888 and A8 stay – layers and font glyphs).
4. **Unused features off:** simple + mono themes, grid layout, observer.
All changed lines in `lv_conf.h` are marked `BLE_TFT 5.5`.

| Build | Flash | Free (496 KB) |
|---|---|---|
| before | 473 KB | 23 KB |
| **after, `-Og`** | **400 KB** | **96 KB** |
| after, LVGL folder `-O2` (faster drawing) | 412 KB | 84 KB |

**LVGL `-O2` in BLE_TFT:** CubeIDE → Project Explorer → right-click **`Middlewares/lvgl`** (the linked LVGL folder) → Properties → C/C++ Build → Settings (Configuration: Debug) → MCU/MPU GCC Compiler → Optimization → **-O2** → Apply and Close → Clean → Build.

### 13.11 Picture from the phone (step 6, 3 Nov 2026)

**Idea.** A 320×240 picture in RGB565 is 320·240·2 = **153 600 bytes** – more than the whole RAM (96 KB). So the board stores nothing: every Bluetooth packet is written to the display the moment it arrives ("streaming"). The phone does all the heavy work (scaling, colour conversion).

```
phone (Chrome)                                   board
photo -> canvas 320x240 -> RGB565, low byte 1st
Control  write  01 40 01 F0 00  (START 320x240) -> set display window once, pause LVGL
Data     write-without-response 244 B  x 630    -> tft_stream_bytes() straight to the pins
                                     <- Status notify every ~10 %  (state, bytes, %)
                                     <- Status DONE  -> LVGL runs again, picture stays
```

**Image service** (`Core/Src/ble_image_service.c`, `Core/Inc/ble_image_service.h`)

| UUID | Name | Properties | Content |
|---|---|---|---|
| `A07E0010-5A1E-4C0D-9E2B-7F3C1D0E2A10` | Image service | | |
| `A07E0011-…` | Control | Write | `01 wL wH hL hH` = START (size, little endian) · `02` = ABORT |
| `A07E0012-…` | Data | Write without response | ≤ 244 bytes RGB565, low byte first, rows top→bottom |
| `A07E0013-…` | Status | Read, Notify | `[0]` state 0 idle/1 receiving/2 done/3 error · `[1]` error · `[2..5]` bytes received · `[6]` % |

Error codes: 1 = screen 7 not open, 2 = bad size (0 or > 320×240), 3 = no data for 2 s or phone disconnected, 4 = aborted. Smaller pictures are centred.

**Why LVGL is paused.** The display has one "window + write pointer". START sets it once; if LVGL drew something in between (a label update, the button focus) it would move the window and the rest of the picture would land in the wrong place. So `main.c` now does `if (!ble_image_busy()) lv_timer_handler();`. After DONE, LVGL runs again but only redraws *changed* areas – and screen 7 stops changing its labels once a picture arrived – so the picture stays. After an error the whole screen is invalidated and LVGL repaints it.

**Changes**

| File | Change |
|---|---|
| `Core/Src/tft.c` / `tft.h` | `tft_stream_begin()` (set window), `tft_stream_bytes()` (bytes → pins, same LUT fast path, `-O2`) |
| `Core/Src/ble_image_service.c` / `.h` | new: service, event handler, timeout |
| `Core/Src/ui_image.c` | new: screen **7. Phone image** (`ui.h`, `ui_home.c` menu entry) |
| `STM32_WPAN/App/app_ble.c` | `#include "ble_image_service.h"`, `ble_image_service_init();` in USER CODE `APP_BLE_Init_3` |
| `Core/Src/main.c` | include + `if (!ble_image_busy()) lv_timer_handler();` |
| `Core/Inc/app_conf.h` | `CFG_BLE_NUM_GATT_ATTRIBUTES` 68 → 76, `CFG_BLE_ATT_VALUE_ARRAY_SIZE` 1344 → 1856 (room for the 3 new values; too small = `aci_gatt_add_char` fails silently and the service is missing) |
| `STM32CubeIDE/.project` | links for `ble_image_service.c`, `ui_image.c` (refresh the project in CubeIDE: F5) |
| `tools/image_sender.html` | new: the phone page |

Test build: 396 KB flash (Debug, LVGL -O2).

**The phone page – `tools/image_sender.html`.** Uses *Web Bluetooth*, which works in **Chrome on Android** (not in iPhone Safari). Chrome allows Bluetooth only on a "secure" page: `https://…` or `http://localhost`. Two ways to open it:

*A – from your PC over Wi-Fi (quickest)*
1. PC: `cd C:\work\stm_projects\BLE_TFT\tools` → `python -m http.server 8000`. Find the PC address with `ipconfig` (e.g. 192.168.1.20). Allow Python in the Windows firewall if asked.
2. Phone Chrome: open `chrome://flags/#unsafely-treat-insecure-origin-as-secure`, enter `http://192.168.1.20:8000`, set **Enabled**, **Relaunch**.
3. Phone Chrome: open `http://192.168.1.20:8000/image_sender.html`.

*B – any https host*: put the single file on GitHub Pages (or any https web space) and open that link. No flag needed.

**Use**
1. Board: menu **7. Phone image** (it accepts pictures only on this screen). Make sure it advertises (screen 5 → **Advertise** if needed). Phone Bluetooth + Location on.
2. Page: **Connect to TFT_xx** → pick the board.
3. **Choose photo** → preview shows exactly what the TFT will show (Fill = crop to 4:3, Whole = black bars; Dither smooths colour steps of 16-bit colour).
4. **Send to display** → the picture builds up line by line on the TFT; bar + KB/s on the phone, board status from notifications.
5. B1 on the board → back to the menu as usual.

**Speed.** Expect roughly 5–20 KB/s → 8–30 s per picture; it depends on the connection interval the phone chooses (each interval carries a few 244-byte packets). If sending fails with an error, pick packet **180** or **20** (some phones use a smaller MTU).

**Troubleshooting**
| Symptom | Cause / fix |
|---|---|
| "Web Bluetooth not available" | not Chrome, or page not secure → method A step 2 or method B |
| No TFT_xx in the chooser | board not advertising → screen 5 **Advertise**; disconnect ST BLE Toolbox first |
| Board: error "screen 7 not open" | open **7. Phone image** on the board, send again |
| Picture shifted / garbage | a packet was lost or LVGL drew in between – send again; check `ble_image_busy()` is in the main loop |
| Image service missing after connect | `CFG_BLE_ATT_VALUE_ARRAY_SIZE` / `NUM_GATT_ATTRIBUTES` too small, or `.project` links missing (not compiled) |

### 13.12 Now playing from the iPhone (step 7, 3 Nov 2026)

**Idea.** iOS has a built-in Bluetooth LE service, the **Apple Media Service (AMS)**. Any app that appears in the iPhone's "Now Playing" controls (Music, Spotify, YouTube, podcasts…) is reported through it – no app needed on the phone. Here the roles are reversed compared to our other services: the **iPhone is the GATT server** and the **board is the GATT client** (`ams_client.c`). Android has no such service.

```
board (client)                                          iPhone (AMS server)
1. find service 89D3502B-0F36-433A-8EF4-C502AD55F8DC  ->
2. find its characteristics                           ->  Remote Command / Entity Update / Entity Attribute
3. write 01 00 to Entity Update CCCD                  ->  "insufficient authentication" (first time)
   -> security request                                ->  popup "Bluetooth Pairing Request" -> Pair
   -> write again                                     ->  OK (link encrypted, bond stored)
4. write 00 00 01   (Player: Name, PlaybackInfo)      ->
5. write 02 00 02 03 (Track: Artist, Title, Duration) ->
6.                                                    <-  notify [entity][attribute][flags][UTF-8 text]
   e.g. 02 02 00 "Blinding Lights"   00 01 00 "1,1.0,42.5" (playing, rate 1.0, 42.5 s)
Buttons: write 2 (play/pause), 3 (next), 4 (previous) to Remote Command
```

**Files / changes**

| File | Change |
|---|---|
| `Core/Src/ams_client.c`, `Core/Inc/ams_client.h` | new: AMS client, step machine driven by GATT events + `ams_client_poll()` |
| `Core/Src/ui_music.c` | new: screen **9. Now playing** (app, play state, title (scrolls), artist, progress, time, ⏮ ⏯ ⏭ buttons, status line) |
| `STM32_WPAN/App/app_ble.c` | include + `ams_client_init()` in `APP_BLE_Init_3`; hooks in USER CODE of `ACI_GAP_NUMERIC_COMPARISON_VALUE` (pairing code shown on screen) and `ACI_GAP_PAIRING_COMPLETE`; scan response with AMS **service solicitation** (USER CODE `ADV_DATA_UPDATE_1`); `ble_ui_conn_handle()` |
| `Core/Inc/ble_ui.h` | `ble_ui_conn_handle()` |
| `Core/Src/main.c` | `ams_client_poll();` in the loop |
| `ui.h`, `ui_home.c` | menu entry 9 |
| `STM32CubeIDE/.project` | links for `ams_client.c`, `ui_music.c` (F5 in CubeIDE) |

Test build: 404 KB code (Debug, LVGL -O2).

**Use**
1. Board: menu **5. Bluetooth** → **Advertise** (if not advertising), then menu **9. Now playing**.
2. iPhone: **Settings → Bluetooth** → under *Other devices* tap **TFT_xx**. (If it is not listed: free app *nRF Connect* or *LightBlue* → connect to TFT_xx.)
3. iPhone popup **Bluetooth Pairing Request** → **Pair** (the code is also shown on the TFT status line).
4. Play music in any app → the TFT shows app, title, artist, time; ⏮ ⏯ ⏭ control the phone (B1 tap = next button, hold = press).
5. Next time the iPhone reconnects by itself when the board advertises (bond is stored in the board's NVM).

**Notes**
- Text is ASCII in our fonts; other characters (é, Hindi, emoji…) show as boxes.
- Long values are cut by iOS to the link's packet size (marked with "...").
- The screen updates only when the iPhone sends a change; the time runs on locally from the last "elapsed" value.
- Status "No Apple media service found": the connected device is not an iPhone, or iOS hides AMS until paired → press **Pair** (middle button).
- To start over: iPhone Settings → Bluetooth → TFT_xx → *Forget This Device*, and reset the board.

### 13.13 Using it with the phone
1. Install **ST BLE Toolbox** (Android / iOS).
2. Board: menu **5. Bluetooth** – status shows *Advertising…* (60 s after reset or after **Advertise**).
3. App: scan → **TFT_xx** → connect → **P2P Server** service.
4. App **LED** button → on-screen blue LED + "Phone LED: ON/OFF (n writes)".
5. Board **Notify phone** → the app shows the "button pressed" notification (notifications must be enabled in the app; status line shows *enabled*).
6. Advertising stops after 60 s without a connection → **Advertise**.

---

## 14. How BLE_TFT is structured (for debugging)

### 14.1 Layers
```
┌────────────────────────────────────────────────────────────┐
│ OUR APPLICATION            Core/Src/ui_*.c                 │  screens, menu
├────────────────────────────────────────────────────────────┤
│ OUR SERVICES   lvgl_port.c  input_port.c  sensors.c  ble_ui│  glue code
├──────────────────────────┬─────────────────────────────────┤
│ LVGL (Core/lvgl)         │ ST BLE APP (STM32_WPAN/App)     │  libraries
│ draws into a buffer      │ app_ble.c, p2p_server(_app).c   │
├──────────────────────────┼─────────────────────────────────┤
│ OUR DISPLAY DRIVER tft.c │ ST BLE STACK (.a libraries)     │
│                          │ + sequencer, timers, adc_ctrl   │
├──────────────────────────┴─────────────────────────────────┤
│ ST HAL / LL drivers  (from the STM32Cube firmware package) │
├────────────────────────────────────────────────────────────┤
│ HARDWARE: GPIO → TFT, radio, ADC4, button B1, LD2          │
└────────────────────────────────────────────────────────────┘
```

### 14.2 Who owns which file
| Owner | Files | Note |
|---|---|---|
| **Ours** | `Core/Src`: tft.c, lvgl_port.c, input_port.c, sensors.c, ui*.c, font; `Core/Inc`: ble_ui.h, lv_conf.h … | change freely |
| **ST + our additions** | main.c, app_conf.h, app_ble.c, p2p_server_app.c, adc_ctrl.c (local copy) | our parts marked **`BLE_TFT`** (Ctrl+H) |
| **ST untouched (copied)** | app_entry.c, p2p_server.c, STM32_WPAN/Target/*, System/* | read / breakpoints only |
| **ST linked from the firmware package** | HAL, BLE stack .a, Utilities (sequencer, timer server), Projects/Common | arrow icon in CubeIDE – never edit |
| **Library** | Core/lvgl | only change lv_conf.h |

### 14.3 Start-up order
```
main()                                              Core/Src/main.c
 ├─ HAL_Init(), MX_APPE_Config() (crystal trim), SystemClock_Config() (32 MHz HSE)
 ├─ MX_GPIO_Init, MX_RTC_Init, MX_ICACHE_Init ...
 ├─ MX_APPE_Init()                                  app_entry.c
 │    ├─ System_Init(): timer server, ADCCTRL_Init(), RNG, flash
 │    └─ APP_BLE_Init()                             app_ble.c
 │         BLE tasks → sequencer, stack + GAP/GATT, P2P_SERVER_APP_Init(), advertising 60 s
 ├─ USER CODE: tft_init → lvgl_port_init → input_port_init → sensors_init → ui_create
 └─ while (1): MX_APPE_Process() (UTIL_SEQ_Run, BLE tasks) ; lv_timer_handler() (LVGL)
```

### 14.4 Run time (no operating system)
| Who | When | What |
|---|---|---|
| Radio interrupts (link layer) | any time | packet timing |
| ST sequencer `UTIL_SEQ_Run` | each loop | BleStack_Process_BG, Ble_UserEvtRx, notifications |
| LVGL `lv_timer_handler` | each loop | B1 read (~30 ms), our lv_timers, redraw |
| Our lv_timers | sensors 1 s, dashboard 0.5 s, Bluetooth screen 0.25 s, Hello 1 s | ADC, labels, chart |
| SysTick | 1 ms | HAL_GetTick() → LVGL clock |

While LVGL draws (up to ~150 ms full screen at 32 MHz) the sequencer waits; radio timing is safe (interrupts). **Avoid long HAL_Delay()** in our code.

### 14.5 Data flows
**Phone LED → screen:** radio → link layer IRQ → BLE stack → `P2P_SERVER_EventHandler()` (p2p_server.c) → `P2P_SERVER_Notification()` case `..._LED_C_WRITE_NO_RESP_EVT` (p2p_server_app.c: Led1, write count – breakpoint here) → ui_ble.c `refresh()` (≤ 0.25 s) → LVGL → `flush_cb` → `tft_push_pixels` → TFT.

**"Notify phone" → phone:** B1 hold → input_port.c `read_cb` (ENTER) → click → ui_ble.c `notify_cb` → `ble_ui_send_notification()` → `UTIL_SEQ_SetTask(CFG_TASK_SEND_NOTIF_ID)` → `P2P_SERVER_Switch_c_SendNotification()` → `P2P_SERVER_UpdateValue()` → `aci_gatt_update_char_value()` → radio.

**Temperature:** lv_timer 1 s → sensors.c `sample()` → `adc_read()` → `ADCCTRL_RequestIpState/RequestValues` (adc_ctrl.c, shared with the radio) → `ADCCTRL_LastRaw[0]`, `ADCCTRL_LastVddMv` → 1-point formula → smoothing → history → ui_dashboard.c.

### 14.6 Where to look
| Symptom | Start here |
|---|---|
| HardFault_Handler | stack 0x2000 in .ld; Debug view call stack; Window → Show View → Fault Analyzer |
| Screen white / garbled | tft.c (`tft_init`, `write8` / `push_fast`), wiring |
| Screen frozen | pause: inside `lv_timer_handler` or `UTIL_SEQ_Run`? timer using a deleted object? |
| B1 not reacting | input_port.c `read_cb()` |
| Phone can't see board | app_ble.c advertising (60 s timeout), Bluetooth screen status |
| LED write not shown | breakpoint p2p_server_app.c `..._LED_C_WRITE_NO_RESP_EVT` |
| Notification not received | `ble_ui_notify_enabled()` must be 1 (enable in the phone app) |
| Temperature wrong | dashboard diagnostics line, sensors.c `sample()`, `ADCCTRL_LastRaw[]` |
| Out of flash / RAM | Build Analyzer (Memory Regions); `LV_MEM_SIZE`, `DRAW_BUF_LINES` |

Useful Live Expressions: `ADCCTRL_LastRaw`, `ADCCTRL_LastVddMv`. Find all our changes in ST files: Ctrl+H → `BLE_TFT`.

---

## 15. BLE_TFT_AutoLock (20–21 Nov 2026)

This folder is the combined project (TFT + laptop AutoLock): menus **10. AutoLock log** and **11. AutoLock**.
Full description: **BLE_TFT_AUTOLOCK.md**. White-screen wiring debug: **TFT_WIRING_DEBUG.md**.

Touch screen (menu 12, D5/D6 wires swapped): see **TOUCH_SETUP.md**.
