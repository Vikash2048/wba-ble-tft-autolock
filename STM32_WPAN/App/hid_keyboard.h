/**
 * hid_keyboard.h - BLE_AutoLock Step 4: the board is a Bluetooth keyboard
 *
 * The laptop pairs with the board like any wireless keyboard (HID over GATT).
 * When the phone goes FAR, the board "presses" Win+L itself - no script,
 * no USB data cable needed.
 */
#ifndef HID_KEYBOARD_H
#define HID_KEYBOARD_H

#include <stdint.h>

void    hid_keyboard_init(void);                 /* add HID + Battery + Device Info services */
void    hid_keyboard_lock(void);                 /* press Win+L (released on next tick)       */
void    hid_keyboard_wake(void);                 /* tap Shift: wake screen (Step 5a)          */
void    hid_keyboard_tick(void);                 /* every 500 ms                              */
uint8_t hid_keyboard_ready(void);                /* 1 = a laptop is subscribed to key reports */
void    hid_keyboard_on_disconnect(uint16_t conn);
void    hid_keyboard_link_known(uint16_t conn, uint8_t is_phone); /* answers a held Report Map read */

#endif /* HID_KEYBOARD_H */
