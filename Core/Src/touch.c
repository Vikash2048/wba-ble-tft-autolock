/* Resistive touch for the TFT shield (BLE_TFT_AutoLock step T4).
 *
 * How a resistive panel works: two transparent resistive layers. Put 3.3 V across
 * one layer (one end 3.3 V, other end 0 V) -> the voltage along it goes 0..3.3 V.
 * Pressing makes the other layer touch it at one point; that layer then carries the
 * voltage of the touch point -> read it with the ADC = position on that axis.
 *
 *   X position: X layer driven (XP = 3.3 V, XM = 0 V), read voltage on YP (ADC)
 *   Y position: Y layer driven (YP = 3.3 V, YM = 0 V), read voltage on XP (ADC)
 *   pressed?  : X layer to 0 V, YP input with pull-up -> pressed pulls it LOW
 *
 * The 4 pins are also display pins (WR, RS, D6, D7) and CS is tied to GND, so the
 * display "listens" all the time. While YP (= WR) floats, the display may take a
 * stray write. We prepare the data pins so such a stray byte is a harmless command
 * (0x5A = unused, 0xDA = "read ID"), and every LVGL screen update starts with a
 * window command (0x2A/0x2B/0x2C) which puts the display back in order.
 * Touch is only read from LVGL (main loop), never during a screen update. */
#include "main.h"
#include "lvgl.h"
#include "touch.h"
#include "adc_ctrl.h"
#include "settings.h"               /* calibration is saved in the settings flash page */

extern uint16_t ADCCTRL_LastRaw[8];        /* our addition in adc_ctrl.c */

/* pins (after the D5/D6 wire swap) */
#define XP_PORT GPIOA
#define XP_PIN  GPIO_PIN_7                 /* LCD_D6, ADC4_IN2 */
#define XM_PORT GPIOC
#define XM_PIN  GPIO_PIN_13                /* LCD_RS            */
#define YP_PORT GPIOA
#define YP_PIN  GPIO_PIN_5                 /* LCD_WR, ADC4_IN4  */
#define YM_PORT GPIOA
#define YM_PIN  GPIO_PIN_6                 /* LCD_D7, ADC4_IN3  */
#define CH_READ_X  LL_ADC_CHANNEL_4        /* sense on YP while the X layer is driven */
#define CH_READ_Y  LL_ADC_CHANNEL_2        /* sense on XP while the Y layer is driven */

#define PRESS_SAMPLES  4

/* ---- ADC through ST's ADC manager (the Bluetooth stack shares ADC4) ---- */
#define TOUCH_ADC_HANDLE(ch) {                                              \
  .Uid = 0x00, .State = ADCCTRL_HANDLE_NOT_REG,                             \
  .InitConf = {                                                             \
    .ConvParams = {                                                         \
      .TriggerFrequencyMode = LL_ADC_TRIGGER_FREQ_LOW,                      \
      .Resolution = LL_ADC_RESOLUTION_12B,                                  \
      .DataAlign = LL_ADC_DATA_ALIGN_RIGHT,                                 \
      .TriggerStart = LL_ADC_REG_TRIG_SOFTWARE,                             \
      .TriggerEdge = LL_ADC_REG_TRIG_EXT_RISING,                            \
      .ConversionMode = LL_ADC_REG_CONV_SINGLE,                             \
      .DmaTransfer = LL_ADC_REG_DMA_TRANSFER_NONE,                          \
      .Overrun = LL_ADC_REG_OVR_DATA_OVERWRITTEN,                           \
      .SamplingTimeCommon1 = LL_ADC_SAMPLINGTIME_39CYCLES_5,                \
      .SamplingTimeCommon2 = LL_ADC_SAMPLINGTIME_1CYCLE_5 },                \
    .SeqParams = { .Setup = LL_ADC_REG_SEQ_CONFIGURABLE,                    \
                   .Length = LL_ADC_REG_SEQ_SCAN_DISABLE,                   \
                   .DiscMode = LL_ADC_REG_SEQ_DISCONT_1RANK },              \
    .LowPowerParams = { .AutoPowerOff = DISABLE,                            \
                        .AutonomousDPD = LL_ADC_LP_AUTONOMOUS_DPD_DISABLE } \
  },                                                                        \
  .ChannelConf = { { .Channel = (ch), .Rank = LL_ADC_REG_RANK_1,            \
                     .SamplingTime = LL_ADC_SAMPLINGTIME_COMMON_1 } }       \
}
static ADCCTRL_Handle_t adc_x = TOUCH_ADC_HANDLE(CH_READ_X);
static ADCCTRL_Handle_t adc_y = TOUCH_ADC_HANDLE(CH_READ_Y);

static uint16_t adc_read(ADCCTRL_Handle_t *h)
{
    int16_t v[1];
    if (ADCCTRL_RequestValues(h, v) != ADCCTRL_OK) return 0;
    return ADCCTRL_LastRaw[0];
}

/* ---- pin helpers ---- */
static void pin_mode(GPIO_TypeDef *p, uint16_t n, uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef g = {0};
    g.Pin = n; g.Mode = mode; g.Pull = pull; g.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(p, &g);
}
static void out(GPIO_TypeDef *p, uint16_t n, int level)    /* set level first, then output */
{
    p->BSRR = level ? n : ((uint32_t)n << 16);
    pin_mode(p, n, GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
}
static void settle(void) { for (volatile int i = 0; i < 400; i++) { } }   /* ~20 us */

/* data pins D0..D5 = 0x1A so a stray write is harmless (see top) */
static void bus_safe(void)
{
    /* D0 PA9=0, D1 PA10=1, D2 PA8=0, D3 PA11=1, D4 PB2=1, D5 PB15=0 (D5 after swap) */
    GPIOA->BSRR = (GPIO_PIN_10 | GPIO_PIN_11) | ((uint32_t)(GPIO_PIN_9 | GPIO_PIN_8) << 16);
    GPIOB->BSRR = GPIO_PIN_2 | ((uint32_t)GPIO_PIN_15 << 16);
}

/* give the 4 pins back to the display: WR high first (no write), RS high, D6/D7 outputs */
static void bus_restore(void)
{
    out(YP_PORT, YP_PIN, 1);       /* WR */
    out(XM_PORT, XM_PIN, 1);       /* RS */
    out(XP_PORT, XP_PIN, 0);       /* D6 */
    out(YM_PORT, YM_PIN, 0);       /* D7 */
}

static int is_pressed(void)
{
    out(XP_PORT, XP_PIN, 0);                          /* X layer to 0 V   */
    out(XM_PORT, XM_PIN, 0);
    pin_mode(YM_PORT, YM_PIN, GPIO_MODE_INPUT, GPIO_NOPULL);
    pin_mode(YP_PORT, YP_PIN, GPIO_MODE_INPUT, GPIO_PULLUP);
    settle();
    return HAL_GPIO_ReadPin(YP_PORT, YP_PIN) == GPIO_PIN_RESET;
}

int touch_read_raw(uint16_t *x, uint16_t *y)
{
    uint32_t sx = 0, sy = 0;
    int pressed;

    bus_safe();
    pressed = is_pressed();
    if (pressed)
    {
        /* each handle must ask for the ADC itself - otherwise its reads are refused
         * (that was the bug: Y always 0) */
        ADCCTRL_RequestIpState(&adc_x, ADC_ON);
        ADCCTRL_RequestIpState(&adc_y, ADC_ON);

        /* X: drive X layer, read on YP */
        out(YP_PORT, YP_PIN, 1);                          /* end any stray write first */
        pin_mode(YM_PORT, YM_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL);
        out(XP_PORT, XP_PIN, 1);
        out(XM_PORT, XM_PIN, 0);
        pin_mode(YP_PORT, YP_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL);
        settle();
        for (int i = 0; i < PRESS_SAMPLES; i++) sx += adc_read(&adc_x);

        /* Y: drive Y layer, read on XP */
        out(YP_PORT, YP_PIN, 1);
        out(YM_PORT, YM_PIN, 0);
        pin_mode(XM_PORT, XM_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL);
        pin_mode(XP_PORT, XP_PIN, GPIO_MODE_ANALOG, GPIO_NOPULL);
        settle();
        for (int i = 0; i < PRESS_SAMPLES; i++) sy += adc_read(&adc_y);

        ADCCTRL_RequestIpState(&adc_x, ADC_OFF);
        ADCCTRL_RequestIpState(&adc_y, ADC_OFF);

        /* still pressed? (a finger lifting during the reading gives wrong values) */
        out(YP_PORT, YP_PIN, 1);
        pressed = is_pressed();
        *x = (uint16_t)(sx / PRESS_SAMPLES);
        *y = (uint16_t)(sy / PRESS_SAMPLES);
    }
    bus_restore();
    return pressed;
}

/* ---- calibration: raw -> screen (linear, with axis swap) ---- */
const int16_t TOUCH_CAL_X[3] = { 24, 319 - 24, 24 };
const int16_t TOUCH_CAL_Y[3] = { 24, 24, 239 - 24 };

static struct { int valid, swap; int32_t ax, bx, ay, by; } cal;  /* s = (a * raw) / 1024 + b */

void touch_set_calibration(const uint16_t rx[3], const uint16_t ry[3],
                           const int16_t sx[3], const int16_t sy[3])
{
    /* points: 0 = top-left, 1 = top-right (screen x changes), 2 = bottom-left (screen y changes) */
    int32_t dxx = (int32_t)rx[1] - rx[0], dyx = (int32_t)ry[1] - ry[0];
    cal.swap = (dyx < 0 ? -dyx : dyx) > (dxx < 0 ? -dxx : dxx);   /* raw Y follows screen X? */
    int32_t r0x = cal.swap ? ry[0] : rx[0], r1x = cal.swap ? ry[1] : rx[1];
    int32_t r0y = cal.swap ? rx[0] : ry[0], r2y = cal.swap ? rx[2] : ry[2];
    if (r1x == r0x || r2y == r0y) { cal.valid = 0; return; }
    cal.ax = ((int32_t)(sx[1] - sx[0]) * 1024) / (r1x - r0x);
    cal.bx = sx[0] - (cal.ax * r0x) / 1024;
    cal.ay = ((int32_t)(sy[2] - sy[0]) * 1024) / (r2y - r0y);
    cal.by = sy[0] - (cal.ay * r0y) / 1024;
    cal.valid = 1;
}

int touch_is_calibrated(void) { return cal.valid; }

int touch_map(uint16_t rx, uint16_t ry, int16_t *sx, int16_t *sy)
{
    if (!cal.valid) return 0;
    int32_t a = cal.swap ? ry : rx, b = cal.swap ? rx : ry;
    int32_t x = (cal.ax * a) / 1024 + cal.bx, y = (cal.ay * b) / 1024 + cal.by;
    if (x < 0) x = 0;
    if (x > 319) x = 319;
    if (y < 0) y = 0;
    if (y > 239) y = 239;
    *sx = (int16_t)x; *sy = (int16_t)y;
    return 1;
}

/* ---- LVGL pointer (works next to the B1 keypad) ---- */
static void read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    static int16_t lx, ly;
    uint16_t rx, ry;
    data->state = LV_INDEV_STATE_RELEASED;
    if (cal.valid && touch_read_raw(&rx, &ry))
    {
        touch_map(rx, ry, &lx, &ly);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    data->point.x = lx;
    data->point.y = ly;
}

void touch_init(void)
{
    (void)ADCCTRL_RegisterHandle(&adc_x);
    (void)ADCCTRL_RegisterHandle(&adc_y);
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, read_cb);

    /* calibrated before? (saved in flash by menu 12) -> touch works right away */
    uint16_t rx[3], ry[3];
    if (settings_get_touch_cal(rx, ry))
        touch_set_calibration(rx, ry, TOUCH_CAL_X, TOUCH_CAL_Y);
}
