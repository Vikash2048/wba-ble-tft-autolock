/**
 * settings.h - BLE_AutoLock Step 5c: settings changeable from the phone,
 * kept in flash so they survive power-off.
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include <stdint.h>

typedef struct
{
  int8_t  near_dbm;     /* above this (smoothed) = NEAR          e.g. -54 */
  int8_t  far_dbm;      /* below this (smoothed) = FAR           e.g. -62 */
  uint8_t far_hold_s;   /* seconds FAR must last before LOCK     e.g. 5   */
  uint8_t near_hold_s;  /* seconds NEAR must last before NEAR    e.g. 2   */
  uint8_t ema_div;      /* smoothing: 4 = new sample counts 25 % e.g. 4   */
  uint8_t pause_min;    /* B1 pause auto-resume, minutes         e.g. 60  */
} settings_t;

void              settings_init(void);           /* load from flash (or defaults) */
void              settings_service_init(void);   /* add the GATT service          */
const settings_t *settings_get(void);
void              settings_tick(void);           /* every 500 ms: flash save state machine */
/* BLE_TFT_AutoLock: touch calibration (raw values of the 3 crosses), kept in the same flash page */
void              settings_save_touch_cal(const uint16_t rx[3], const uint16_t ry[3]);
int               settings_get_touch_cal(uint16_t rx[3], uint16_t ry[3]);   /* 1 = saved */
void              settings_update_status(uint8_t near, int8_t filt, int8_t raw,
                                         uint8_t paused, uint8_t kbd);

#endif /* SETTINGS_H */
