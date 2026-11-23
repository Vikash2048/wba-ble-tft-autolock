#ifndef UI_H
#define UI_H
#include "lvgl.h"

/* Our 28 px font (BLE_TFT step 5.5): Montserrat ASCII + degree sign + Bluetooth icon.
 * Replaces LVGL's built-in lv_font_montserrat_28 (which carries ~60 icons, 36 KB). */
LV_FONT_DECLARE(lv_font_montserrat_28_deg);

/* Entry point: shows the home menu. Call after lvgl_port_init() and input_port_init(). */
void ui_create(void);

/* Screens (one file each). Every feature we add gets its own screen + menu entry. */
void ui_show_home(void);      /* ui_home.c    - menu                       */
void ui_show_hello(void);     /* ui_hello.c   - step 0: Hello LVGL         */
void ui_show_buttons(void);   /* ui_buttons.c - step 1: button control     */
void ui_show_dashboard(void); /* ui_dashboard.c - step 2: live sensor dashboard */
void ui_show_speed(void);     /* ui_speed.c   - step 3: redraw speed test       */
void ui_show_ble(void);       /* ui_ble.c     - step 4: Bluetooth (BLE_TFT only) */
void ui_show_message(void);   /* ui_message.c - step 5.3: text from the phone (BLE_TFT only) */
void ui_show_image(void);     /* ui_image.c   - step 6: picture from the phone (BLE_TFT only) */
void ui_show_music(void);
void ui_show_autolock(void);
void ui_show_autolock_view(void);
void ui_show_touch(void);
void ui_show_clock(void);          /* ui_clock.c - smart desk clock (time from the iPhone) */          /* ui_touch.c - touch test + calibration */  /* ui_autolock_view.c - BLE_TFT_AutoLock step C: NEAR/FAR, graph, pause, forget */  /* ui_autolock.c - BLE_TFT_AutoLock: status + log on screen */     /* ui_music.c   - step 7: now playing on the iPhone (BLE_TFT only) */

/* Helpers (ui.c) */
lv_obj_t *ui_new_screen(const char *title);          /* black screen + title       */
lv_obj_t *ui_add_back_button(lv_obj_t *scr);         /* "< Menu" button, top-left  */
void      ui_load(lv_obj_t *scr, lv_obj_t *focus);   /* show screen, delete old    */
void      ui_go(void (*show_fn)(void));              /* switch screen from an event */

#endif
