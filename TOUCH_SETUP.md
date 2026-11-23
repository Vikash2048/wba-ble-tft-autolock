# Touch screen on the TFT shield – how it works and how we made it

**Project:** `C:\work\stm_projects\BLE_TFT_AutoLock` · **Date:** 21–23 Nov 2026
**Result:** resistive touch works on every screen (next to button B1), calibration saved in flash.

---

## 1. Summary

| | |
|---|---|
| Touch type | **4-wire resistive** film on top of the display |
| Pins | shares 4 display pins: **X layer = LCD_D6 ↔ LCD_RS**, **Y layer = LCD_WR ↔ LCD_D7** |
| Hardware change | **D5 and D6 wires swapped** at the Nucleo (D6 needed an ADC pin) |
| Software | `touch.c` (read + calibrate + LVGL pointer), `ui_touch.c` (menu **12. Touch**) |
| Calibration | 3 crosses (top-left, top-right, bottom-left), **saved in flash** |
| B1 | still works (tap = next, hold = select) |

---

## 2. How a resistive touch panel works

Two transparent layers with a resistive coating, separated by tiny spacer dots:

```
   X layer:  XP ●━━━━━━━━━━━━━━━━━━━━━━━● XM      3.3 V ... 0 V along the layer
                          ▼ finger presses here
   Y layer:  YP ●━━━━━━━━━━━━━━━━━━━━━━━● YM      picks up the voltage at that point
```

- **X position:** put 3.3 V on XP and 0 V on XM → the voltage along the X layer goes
  from 3.3 V to 0 V. Where you press, the Y layer touches it and carries that voltage →
  read it on **YP with the ADC** → a number 0…4095 = position.
- **Y position:** the same the other way round: drive the Y layer, read on **XP**.
- **Pressed or not:** put 0 V on the X layer, YP as input with pull-up. Not pressed → YP
  reads HIGH. Pressed → the layers touch → YP pulled LOW.

The ADC pin must be on the layer that is **not** driven. That is why both reading pins
(YP and XP) must be ADC-capable.

---

## 3. Which pins (found with the firmware tests)

| Touch | Display pin | Board pin | ADC |
|---|---|---|---|
| XP (X+) | LCD_D6 | **PA7** (CN4-6, after swap) | ADC4_IN2 |
| XM (X−) | LCD_RS | PC13 (CN4-30) | – |
| YP (Y+) | LCD_WR | PA5 (CN6-3) | ADC4_IN4 |
| YM (Y−) | LCD_D7 | PA6 (CN3-37) | ADC4_IN3 |

How we knew: the wiring test (`TFT_PIN_TEST 1`) showed D6↔RS and WR↔D7 as "touching"
(~300–900 Ω – the two layers), and the touch test (`TFT_PIN_TEST 4`) confirmed both
layers and lit LD2 when the screen was pressed. **No multimeter needed.**

### The wire swap

D6 was on PB15, which has **no ADC**. PA7 (used by D5) has one, so the two wires swapped:

| TFT pin | before | after |
|---|---|---|
| LCD_D5 | CN4-6 (PA7) | **CN4-38 (PB15)** |
| LCD_D6 | CN4-38 (PB15) | **CN4-6 (PA7)** |

`tft.c` was changed to match (`DATA[]` table: D5 = PB15, D6 = PA7).
**If the code and the wires don't match, the screen stays white** (bits 5 and 6 of every
command are mixed up – the "display ON" command never arrives).

---

## 4. Sharing pins with the display – the trick

The 4 touch pins are display pins, and **CS is tied to GND**, so the display listens all
the time. While YP (= WR) floats during a reading, the display may see a "write" pulse.
Moving CS to a free pin (PA12) would avoid that, but its header pin was unknown.

What we do instead:

1. Touch is read **only from LVGL** (main loop), never while a screen update is running.
2. Before reading, data pins D0–D5 are set to `0x1A`. A stray write then becomes command
   `0x1A`, `0x5A`, `0x9A` (unused) or `0xDA` (read ID) – all **harmless**.
3. WR is always driven **HIGH first** when giving the pins back (no extra write pulse).
4. Every LVGL screen update starts with window commands (`0x2A`, `0x2B`, `0x2C`), which
   put the display back in order anyway.

---

## 5. Software

### `Core/Src/touch.c`

| Function | Does |
|---|---|
| `touch_init()` | registers 2 ADC handles, adds the LVGL **pointer** input, loads the saved calibration |
| `touch_read_raw(&x, &y)` | pressed? → read X (4 samples) → read Y (4 samples) → still pressed? → give pins back |
| `touch_set_calibration()` | computes raw → screen formula from 3 points |
| `touch_map()` | raw → screen pixel (0…319, 0…239) |

**ADC:** the Bluetooth stack also uses ADC4 (radio temperature), so we go through ST's
ADC manager (`adc_ctrl.c`): one handle per channel, **each handle must request
`ADC_ON` itself** – otherwise its reads are refused (that was the "Y never changes" bug).

### `Core/Src/ui_touch.c` – menu 12. Touch

- Live raw values (`raw X 1850 raw Y 2210 PRESSED`) – to check the panel.
- Green dot where you touch (after calibration).
- **Calibrate:** tap the 3 red crosses one after the other (press, then lift).

### Calibration (3 points)

A resistive panel is close to linear, so each axis needs `screen = a × raw + b`:

| Crosses | Gives |
|---|---|
| top-left → top-right | X scale + offset (only screen X changes) |
| top-left → bottom-left | Y scale + offset |
| comparing both | whether raw X/Y are **swapped** relative to the screen |

Points at (24, 24), (295, 24), (24, 215). Fixed-point maths (`a` × 1024), no float.
A 5-point version (4 corners + centre, least squares, accuracy check) is possible
if the dot is ever off near the edges – 3 points are accurate enough on this panel.

### Saved in flash

The calibration lives in the **same 4 KB flash page as the AutoLock settings**
(`settings.c`). The record grew to 32 bytes, magic `"ALK2"`:

```
magic | settings (6 B) | cal_valid | pad | cal_rx[3] | cal_ry[3] | pad
```

The old 16-byte record (`"ALK1"`) is still read, so existing settings are kept.
At power-on `touch_init()` loads it → touch works without calibrating again.

### Changed files

| File | Change |
|---|---|
| `Core/Src/touch.c`, `Core/Inc/touch.h` | **new** |
| `Core/Src/ui_touch.c` | **new** – menu 12 |
| `Core/Src/tft.c` | D5/D6 pins swapped |
| `Core/Src/main.c` | `touch_init()` after `input_port_init()`; test mode 4 (touch test); test tables updated for the swap |
| `STM32_WPAN/App/settings.c/.h` | 32-byte record, `settings_save_touch_cal()`, `settings_get_touch_cal()` |
| `Core/Inc/ui.h`, `Core/Src/ui_home.c` | menu entry 12 |
| `STM32CubeIDE/.project` | links for `touch.c`, `ui_touch.c` |

---

## 6. Steps we went through

| Step | What | Result |
|---|---|---|
| T1 | Touch test without multimeter (`TFT_PIN_TEST 4`): layers found? LD2 on when pressed? | both layers found, LD2 follows finger |
| T2 | Decide: CS move (PA12) or not | not needed – stray writes made harmless (§4) |
| T3 | Swap D5/D6 wires + `tft.c` | white screen until the wires really matched the code |
| T4 | `touch.c` + menu 12 | raw X changed, **Y always 0** |
| fix | each ADC handle requests `ADC_ON` | X and Y change, calibration works |
| T5 | save calibration in flash | works after power-off |

---

## 7. Troubleshooting

| Symptom | Cause | Fix |
|---|---|---|
| White screen after the touch update | D5/D6 wires not swapped (or only one moved) | LCD_D5 → CN4-38, LCD_D6 → CN4-6 |
| Raw X/Y never change | wrong ADC channel, or ADC handle not switched on | check §3 channels; each handle `ADC_ON` |
| Only X (or only Y) changes | one handle's reads refused / wrong channel | see above |
| "Calibration failed" | one axis gives the same value at two crosses | fix the raw readings first |
| Dot is offset | calibration taps were not on the cross centres | Calibrate again, tap precisely (fingernail/stylus) |
| Taps do nothing | not calibrated (pointer is off until then) | menu 12 → Calibrate |
| Odd pixels after touching | stray write | should not happen (§4); a screen change redraws everything |
| Test modes | `#define TFT_PIN_TEST` in `main.c`: 1 wiring, 2 colour, 3 ID read, **4 touch** – set back to 0 |
