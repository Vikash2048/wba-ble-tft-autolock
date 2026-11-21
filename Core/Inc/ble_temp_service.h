#ifndef BLE_TEMP_SERVICE_H
#define BLE_TEMP_SERVICE_H
#include <stdint.h>

/* Step 5.2 – standard Bluetooth "Environmental Sensing" service (UUID 0x181A)
 * with one "Temperature" characteristic (UUID 0x2A6E): Read + Notify,
 * value = signed 16-bit, unit 0.01 deg C (31.62 C is sent as 3162).
 * Any Bluetooth app (ST BLE Toolbox, nRF Connect ...) can show it. */

void     ble_temp_service_init(void);          /* called in APP_BLE_Init (app_ble.c) */
void     ble_temp_service_update(float temp_c);/* new value -> phone (notification if enabled) */
float    ble_temp_service_last(void);          /* last value sent                  */
uint32_t ble_temp_service_updates(void);       /* number of successful updates     */

#endif
