# TFT white screen – what happened, how we found it, how to fix it next time

**Date:** 20 Nov 2026
**Hardware:** NUCLEO-WBA25CE1 + 2.4" 8-bit parallel TFT shield (ILI9341-compatible), connected with jumper wires
**Project:** `C:\work\stm_projects\BLE_TFT_AutoLock` (test code in `Core\Src\main.c`)

---

## 1. Summary (one minute version)

> **Correction (21 Nov 2026):** the "D6 ↔ RS touching" result was **not a fault**. On this shield
> the resistive **touch panel** is wired across display pins: **X layer = LCD_D6 ↔ LCD_RS**,
> **Y layer = LCD_WR ↔ LCD_D7** (each ~300–900 Ω). The wiring test sees that resistance and
> reports "touching" – that is normal. **The only real fault was the open D7 wire.**
> (`TFT_PIN_TEST 4` = touch test confirms both layers.)


| | |
|---|---|
| **Symptom** | Screen completely **white** (backlight on, nothing drawn) – also with the old, known-good BLE_TFT firmware |
| ~~Root cause 1~~ | ~~D6 and RS touching~~ → **not a fault: touch panel X layer** (see correction) |
| **Root cause** | **D7 wire not connected** (broken / loose jumper between Nucleo CN3-37 and the shield) |
| **Fix** | Replaced the D7 jumper wire (D6 was replaced too, but was fine) |
| **How we found it** | Firmware **test modes** (no multimeter guessing): wiring test → colour test → asking the display for its ID |
| **Key clue** | Every byte read back from the display had its **top bit stuck at 1** (`80 B1 A9` instead of `00 93 41`) → bit 7 = D7 wire |

**Conclusion:** the software was never the problem. A white screen with backlight means *"the display
has power but does not understand the commands"* – almost always a **wiring** problem.

---

## 2. What happened – timeline

1. **Shield re-connected** to the board while the **BLE_autolock** firmware (no TFT code) was flashed.
2. The log showed **"ghost" B2 presses** (`>> B2: keep holding...` / `released`) and pairing failures:
   ```
   Pairing KO  Status 0x03  Reason 0x06      (board has no keys for this device)
   Disconnection Reason 0x13                  (the other device hung up)
   ```
   - On NUCLEO-WBA25 **button B2 = PC13 = LCD_RS**, and **LED LD1 = PA7 = LCD_D5**.
   - The shield moved PC13 up/down → firmware thought B2 was held → after 3 s it **erased all pairings**
     (AutoLock step 5d feature) → phone/laptop still had old keys → pairing failed in a loop.
   - **Lesson:** with the TFT connected, **B2 and the board LEDs must not be used**
     (BLE_TFT has `CFG_BUTTON_SUPPORTED 0` and `CFG_LED_SUPPORTED 0` for this reason).
3. Created the new combined project **BLE_TFT_AutoLock** (copy of BLE_TFT), flashed it → **white screen**.
4. Checked the build: same size as the last working BLE_TFT (393,228 bytes code) → **code identical**.
5. Flashed the **old BLE_TFT** → also white → **hardware problem confirmed**.
6. Multimeter continuity + short checks by hand → nothing obvious found (hard to do with 10+ wires).
7. Added **test modes** to the firmware (section 4) and ran them one after the other.
8. **Wiring test v1:** D6, WR, RS reported as "shorted" to everything – confusing.
9. **Wiring test v2** (better): only **D6 ↔ RS touching**, WR "pulled up by the shield (normal)".
10. Removed the D6 wire → RS OK → **new D6 wire** → short gone. Walk test: all 10 pins 3.3 V.
11. Still white. CS = 0 V, RD = 3.3 V, RST = 3.3 V all correct.
12. **Colour test:** LED LD2 blinking (program runs) but screen white → display ignores commands.
13. **ID read test:** display answered `80 80 80 | 80 B1 A9 | 80` → **bit 7 always 1** → D7 problem.
14. **New D7 wire** → `power(0A) 9C` = display awake and ON → normal firmware → **menu back**.

---

## 3. How the display bus works (why one wire = white screen)

The board talks to the display over an **8-bit parallel bus**:

| Signal | Job |
|---|---|
| **D0–D7** | 8 data wires = one byte at a time (D0 = bit 0 … **D7 = bit 7**, the top bit) |
| **WR** | "write" clock: each low→high pulse = "take this byte now" |
| **RS** (also called CD / DC) | 0 = the byte is a **command**, 1 = the byte is **data** |
| **CS** | chip select – tied to GND = always selected |
| **RD** | "read" – tied to 3.3 V = never read (we only write) |
| **RST** | reset – tied to 3.3 V = reset only at power-up |

Start-up sends a few commands: `0x01` reset, `0x11` sleep out, `0x3A 0x55` colour mode,
`0x36 0xE8` orientation, **`0x29` display ON**. If **one data wire is wrong**, every command
arrives changed:

```
board sends 0x29 (display ON)  = 0010 1001
D7 stuck at 1  → display gets  = 1010 1001 = 0xA9  → unknown command → stays OFF → white
```

Two wires touching (D6 + RS) corrupts both data bits and the command/data decision → also white.

---

## 4. The test modes (in `Core\Src\main.c`)

Near the top of `main.c`:

```c
#define TFT_PIN_TEST 0     /* 0 = normal app, 1 = wiring test, 2 = colour test, 3 = ID read test */
```

Set the number, build, flash, open **Tera Term (COM462, 115200)**, press reset.
**Always set it back to 0** for normal use.

> UART TX is PA6 = LCD_D7, so the normal app has no serial log; the test modes switch PA6
> between UART and D7 themselves.

### Mode 1 – Wiring test (no display needed)

**Part 1 – automatic check of every signal wire** (D0–D7, WR, RS):

| Check | How | Result |
|---|---|---|
| **STUCK** | drive the pin LOW for a moment, read it back | still HIGH → wire is on 3.3 V / 5 V |
| **PULLED** | all pins inputs with weak pull-down | HIGH → shield has a pull-up resistor (**normal**, harmless) |
| **TOUCHING** | all pins inputs with pull-UP, drive one LOW | another one also LOW → the two wires touch – **except D6↔RS and WR↔D7 = touch panel layers (normal)** |

Output example (what we saw):

```
   D6  board PB15 CN4-38 : BAD  - touching: RS
   WR  board PA5  CN6-3  : ok   (pulled up by the shield - normal)
   RS  board PC13 CN4-30 : BAD  - touching: D6
```

**Part 2 – walk test (you measure):** each signal is 3.3 V for 6 s, all others 0 V.
Green LED LD2 blinks N times before signal N, then stays on.
Multimeter DC volts, black probe GND, red probe **on the TFT shield pin**:
- named pin = **3.3 V** → good
- **also check the other pins show 0 V** – a pin that shows 3.3 V *all the time* is not
  connected (the shield's pull-up holds it high). We missed this for D7 at first.

### Mode 2 – Colour test (display only, no Bluetooth, no LVGL)

Starts the display and fills the screen RED, GREEN, BLUE, BLACK, WHITE (1 s each), LD2 toggles.

| Screen | LD2 | Meaning |
|---|---|---|
| colours change | blinking | display + wiring OK → problem is in the app |
| white | blinking | program runs, display ignores commands → wiring / display |
| white | not blinking | program not running (flash / debug problem) |

### Mode 3 – Ask the display who it is (ID read test)

Needs **one wire moved**: shield **LCD_RD** from IOREF (CN5-2) to **CN4-36** (PA1 = button B1,
don't press it). Put it back afterwards!

Every 2 s the board reads back:

| Read | Command | Good answer |
|---|---|---|
| ID | `0xD3` | `00 93 41` on a genuine ILI9341 (our clone answers `00 31 29` – fine) |
| **Power mode** | `0x0A` | **`9C`** = awake + display ON + normal mode |
| Status | `0x09` | e.g. `F4 53 04 00` |

How to read the answers:

| Answer | Meaning |
|---|---|
| all `FF` or all `00` | display does not answer (no logic power, CS not low, RD not moved) |
| **same bit wrong in every byte** | **that data wire is broken/stuck** (bit 7 = D7, bit 6 = D6 …) |
| `power 80` / `00` | display still asleep → start-up commands not understood |
| `power 9C` | display accepted the commands ✔ |

What we got:

```
before fix:  ID(D3) 80 B1 A9 | power(0A) 80      ← top bit (0x80) set everywhere → D7
after fix:   ID(D3) 00 31 29 | power(0A) 9C      ← display ON
```

---

## 5. Wiring reference (final, working)

| TFT pin | Board pin | Nucleo connector |
|---|---|---|
| LCD_D0 | PA9 | CN6-4 |
| LCD_D1 | PA10 | CN6-5 |
| LCD_D2 | PA8 | CN6-6 |
| LCD_D3 | PA11 | CN6-9 |
| LCD_D4 | PB2 | CN6-10 |
| LCD_D5 | PB15 | CN4-38 *(since 21 Nov: swapped with D6 for touch – see TOUCH_SETUP.md)* |
| LCD_D6 | PA7 | CN4-6 *(since 21 Nov: on the ADC pin for touch)* |
| LCD_D7 | PA6 | CN3-37 *(replaced wire)* |
| LCD_WR | PA5 | CN6-3 |
| LCD_RS | PC13 | CN4-30 |
| LCD_CS | GND | CN5-7 |
| LCD_RD | IOREF 3.3 V | CN5-2 |
| LCD_RST | 3V3 | CN3-16 |
| 3V3 / 5V / GND | | CN5-4 / CN5-5 / CN5-6 |

Pins shared with board functions (side effects):

| Pin | TFT use | Board function | Effect |
|---|---|---|---|
| PC13 | LCD_RS | button **B2** | ghost B2 presses if firmware uses B2 |
| PA7 | LCD_D5 | LED **LD1** | LED flickers; firmware must not drive LD1 |
| PB15 | LCD_D6 | LED **LD3** | LED flickers |
| PA6 | LCD_D7 | **UART TX** (ST-LINK COM port) | no serial log in the normal app |
| PA1 | (RD in mode 3 only) | button **B1** | B1 = LVGL menu key in the app |

---

## 6. Checklist for next time (white screen)

1. **Power-cycle** (unplug USB 5 s) – the display resets only at power-up (RST tied to 3.3 V).
2. Flash a **known-good** firmware (old BLE_TFT). Still white → hardware, not code.
3. **Mode 1** (wiring test) → fix every `BAD` line (touching / stuck).
4. Walk test: each pin 3.3 V **only during its own step**, 0 V otherwise.
5. Measure on the shield: **CS 0 V, RD 3.3 V, RST 3.3 V, 5V ≈ 5 V**.
6. **Mode 2** (colour test): colours → app problem; white → go on.
7. **Mode 3** (ID read): look for a bit that is wrong in every byte → that D-wire.
8. Replace suspect jumper wires (they break inside or sit loose) rather than re-plugging.
9. Back to `TFT_PIN_TEST 0`, RD back to CN5-2, power-cycle.

---

## 7. Lessons learned

- **White screen + backlight = commands not understood.** Look at wiring first, not code.
- **Prove code vs hardware early:** flash a known-good firmware; compare build sizes.
- **Let the firmware test the wiring** – faster and more reliable than probing 13 wires by hand.
- A pull-up on the shield can make a **disconnected** wire look fine on the multimeter (always 3.3 V).
- **Reading back from the device** (ID, power mode) turns guessing into facts – one stuck bit
  pointed straight at the D7 wire.
- Jumper wires fail: broken inside, loose in the header, or touching neighbours. Replace, don't trust.
- Pins are shared: with the TFT connected, **don't use B2, LD1, LD3, UART log** in firmware.
