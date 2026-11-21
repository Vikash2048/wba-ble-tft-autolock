#ifndef BLE_TEXT_SERVICE_H
#define BLE_TEXT_SERVICE_H
#include <stdint.h>

/* Step 5.3 – our own "Message" service: the phone writes text, the TFT shows it.
 *   Service        A07E0001-5A1E-4C0D-9E2B-7F3C1D0E2A10
 *   Characteristic A07E0002-5A1E-4C0D-9E2B-7F3C1D0E2A10  Write / Write without response / Read
 *                  up to BLE_TEXT_MAX bytes of UTF-8 text (ASCII shown, other characters as boxes) */

#define BLE_TEXT_MAX  100

void        ble_text_service_init(void);    /* called in APP_BLE_Init (app_ble.c) */
const char *ble_text_last(void);            /* last message ("" if none)          */
uint32_t    ble_text_count(void);           /* number of messages received        */
uint32_t    ble_text_time_ms(void);         /* HAL_GetTick() when it arrived      */

#endif
