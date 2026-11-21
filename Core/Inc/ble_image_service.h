#ifndef BLE_IMAGE_SERVICE_H
#define BLE_IMAGE_SERVICE_H
#include <stdint.h>

/* Step 6 – "Image" service: the phone sends a picture, it is drawn straight onto the TFT.
 *
 *   Service  A07E0010-5A1E-4C0D-9E2B-7F3C1D0E2A10
 *   Control  A07E0011-...  Write             0x01 w_lo w_hi h_lo h_hi = START (size in pixels)
 *                                            0x02                     = ABORT
 *   Data     A07E0012-...  Write w/o resp.   up to BLE_IMAGE_CHUNK bytes of RGB565,
 *                                            low byte first, row by row, left to right
 *   Status   A07E0013-...  Read + Notify     8 bytes, see BLE_IMG_ST_* below:
 *                                            [0] state [1] error [2..5] bytes received (LE)
 *                                            [6] percent [7] 0
 *
 * A 320x240 picture is 153 600 bytes – more than the whole RAM – so nothing is stored:
 * every chunk goes to the display the moment it arrives (tft_stream_bytes). */

#define BLE_IMAGE_CHUNK   244          /* 247-byte MTU minus 3 bytes ATT header */

/* status[0] */
#define BLE_IMG_ST_IDLE       0
#define BLE_IMG_ST_RECEIVING  1
#define BLE_IMG_ST_DONE       2
#define BLE_IMG_ST_ERROR      3
/* status[1] when state = ERROR */
#define BLE_IMG_ERR_NOT_OPEN  1        /* screen "7. Phone image" is not open on the board */
#define BLE_IMG_ERR_BAD_SIZE  2        /* width/height 0 or larger than 320x240           */
#define BLE_IMG_ERR_TIMEOUT   3        /* no data for 2 s                                 */
#define BLE_IMG_ERR_ABORTED   4        /* phone sent ABORT                                */

void     ble_image_service_init(void);     /* called in APP_BLE_Init (app_ble.c)          */

/* Called from main loop: 1 while a picture is being received -> skip lv_timer_handler().
 * Also handles the 2 s timeout and asks LVGL to redraw after an error. */
int      ble_image_busy(void);

/* Used by ui_image.c */
void     ble_image_set_screen_open(int open); /* only accept pictures while the screen is open */
uint8_t  ble_image_state(void);                /* BLE_IMG_ST_*                                  */
uint8_t  ble_image_error(void);                /* BLE_IMG_ERR_*                                 */
uint32_t ble_image_count(void);                /* pictures completed                            */
uint32_t ble_image_last_ms(void);              /* how long the last picture took                */
uint16_t ble_image_width(void);
uint16_t ble_image_height(void);

#endif
