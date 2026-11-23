#ifndef TOUCH_H
#define TOUCH_H
#include <stdint.h>

/* Resistive touch on the TFT shield. The touch film shares 4 display pins:
 *   X layer: XP = LCD_D6 (PA7, ADC4_IN2 - after swapping the D5/D6 wires)
 *            XM = LCD_RS (PC13)
 *   Y layer: YP = LCD_WR (PA5, ADC4_IN4)
 *            YM = LCD_D7 (PA6, ADC4_IN3)
 * Reading touch borrows these pins between two screen updates, then gives them back. */

void    touch_init(void);                         /* after input_port_init(): adds the LVGL pointer */
int     touch_read_raw(uint16_t *x, uint16_t *y); /* 1 = pressed; raw 12-bit ADC values           */
int     touch_map(uint16_t rx, uint16_t ry, int16_t *sx, int16_t *sy); /* raw -> screen, 0 = not calibrated */

/* calibration with 3 points (see ui_touch.c) */
void    touch_set_calibration(const uint16_t rx[3], const uint16_t ry[3],
                              const int16_t sx[3], const int16_t sy[3]);
int     touch_is_calibrated(void);

/* the 3 calibration crosses on screen: top-left, top-right, bottom-left */
extern const int16_t TOUCH_CAL_X[3], TOUCH_CAL_Y[3];

#endif
