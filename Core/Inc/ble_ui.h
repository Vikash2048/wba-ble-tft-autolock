#ifndef BLE_UI_H
#define BLE_UI_H
#include <stdint.h>

/* Link between ST's Bluetooth application code and our "5. Bluetooth" screen.
 * Implemented inside USER CODE blocks of ST's files:
 *   app_ble.c         -> state + advertising
 *   p2p_server_app.c  -> LED written by the phone + notifications to the phone */

typedef enum {
    BLE_UI_IDLE = 0,       /* not advertising, not connected (press "Advertise") */
    BLE_UI_ADVERTISING,    /* visible to phones                                   */
    BLE_UI_CONNECTED       /* a phone is connected                                */
} ble_ui_state_t;

ble_ui_state_t ble_ui_get_state(void);
void           ble_ui_start_advertising(void);   /* 60 s, like B1 in ST's example */
void           ble_ui_device_name(char *buf, uint32_t size);  /* e.g. "TFT_3A" */

uint8_t        ble_ui_led(void);                 /* LED state written by the phone (0/1) */
uint32_t       ble_ui_led_writes(void);          /* how many times the phone wrote it    */
int            ble_ui_notify_enabled(void);      /* phone subscribed to notifications?   */
int            ble_ui_send_notification(void);   /* "button pressed" to the phone; 0 = not possible */
uint32_t       ble_ui_notifications_sent(void);
uint16_t       ble_ui_conn_handle(void);         /* step 7: handle of the connected phone */

#endif
