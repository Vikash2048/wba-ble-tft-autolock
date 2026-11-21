/* Step 6 – "Phone image" screen.
 * While this screen is open the board accepts pictures from the phone
 * (ble_image_service.c). The picture is drawn directly on the display, over this
 * screen. Afterwards we must not change any label: LVGL would repaint that area
 * and cut a hole into the picture. So the labels are only updated while no
 * picture is on the screen. Press B1 (tap) / hold for "< Menu" as usual. */
#include "main.h"
#include "ui.h"
#include "ble_ui.h"
#include "ble_image_service.h"

static lv_obj_t   *status_label;
static lv_timer_t *refresh_timer;
static uint32_t    count_at_open;

static void refresh(void)
{
    /* a picture arrived since the screen was opened -> it is on the display now:
       touch nothing. (After an error the service has already let LVGL repaint.) */
    if (ble_image_count() != count_at_open && ble_image_state() == BLE_IMG_ST_DONE)
        return;

    const char *conn = (ble_ui_get_state() == BLE_UI_CONNECTED) ? "Phone connected"
                                                                : "Not connected";
    if (ble_image_state() == BLE_IMG_ST_ERROR) {
        static const char *const why[] = { "", "screen not open", "bad size",
                                           "timeout / disconnected", "aborted" };
        uint8_t e = ble_image_error();
        lv_label_set_text_fmt(status_label, "%s - last transfer failed: %s", conn,
                              e < 5 ? why[e] : "?");
    } else {
        lv_label_set_text_fmt(status_label, "%s - waiting for a picture", conn);
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void screen_deleted_cb(lv_event_t *e)
{
    (void)e;
    ble_image_set_screen_open(0);
    lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
}

void ui_show_image(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, screen_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *back = ui_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28_deg, 0);
    lv_label_set_text(title, "Phone image");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 40, 10);

    lv_obj_t *help = lv_label_create(scr);
    lv_obj_set_width(help, 300);
    lv_label_set_long_mode(help, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(help, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(help, lv_color_hex(0xC0C0C0), 0);
    lv_label_set_text(help,
        "1. Open the sender page in Chrome on Android\n"
        "2. Connect to TFT_xx\n"
        "3. Pick a photo and press Send\n\n"
        "The picture (320 x 240) is drawn straight onto the display "
        "while it arrives. Press B1 to go back to the menu.");
    lv_obj_align(help, LV_ALIGN_TOP_MID, 0, 60);

    status_label = lv_label_create(scr);
    lv_obj_set_width(status_label, 300);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x00D0FF), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_LEFT, 10, -12);

    count_at_open = ble_image_count();
    refresh();
    refresh_timer = lv_timer_create(refresh_timer_cb, 250, NULL);
    ble_image_set_screen_open(1);
    ui_load(scr, back);
}
