/* Step 4 – Bluetooth screen (BLE_TFT project only).
 * Phone app: ST BLE Toolbox -> device "TFT_xx" -> P2P Server.
 *  - phone LED button  -> on-screen LED + counter
 *  - "Notify phone"    -> "button pressed" notification on the phone
 *  - "Advertise"       -> become visible again (advertising stops after 60 s) */
#include "ui.h"
#include "ble_ui.h"
#include "ble_temp_service.h"

static lv_obj_t   *state_label, *led, *led_label, *notif_label, *temp_label;
static lv_timer_t *refresh_timer;

static void refresh(void)
{
    switch (ble_ui_get_state()) {
    case BLE_UI_CONNECTED:
        lv_label_set_text(state_label, "Connected");
        lv_obj_set_style_text_color(state_label, lv_color_hex(0x00E040), 0);
        break;
    case BLE_UI_ADVERTISING:
        lv_label_set_text(state_label, "Advertising...");
        lv_obj_set_style_text_color(state_label, lv_color_hex(0xFFD000), 0);
        break;
    default:
        lv_label_set_text(state_label, "Idle - press Advertise");
        lv_obj_set_style_text_color(state_label, lv_color_hex(0x909090), 0);
        break;
    }

    if (ble_ui_led()) lv_led_on(led); else lv_led_off(led);
    lv_label_set_text_fmt(led_label, "Phone LED: %s  (%lu writes)",
                          ble_ui_led() ? "ON" : "OFF", (unsigned long)ble_ui_led_writes());
    int32_t t10 = (int32_t)(ble_temp_service_last() * 10.0f + 0.5f);
    lv_label_set_text_fmt(temp_label, "Temperature to phone: %ld.%ld C  (%lu updates)",
                          (long)(t10 / 10), (long)(t10 % 10), (unsigned long)ble_temp_service_updates());
    lv_label_set_text_fmt(notif_label, "Notifications: %s   sent: %lu",
                          ble_ui_notify_enabled() ? "enabled" : "off",
                          (unsigned long)ble_ui_notifications_sent());
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void adv_cb(lv_event_t *e)    { (void)e; ble_ui_start_advertising(); refresh(); }
static void notify_cb(lv_event_t *e) { (void)e; ble_ui_send_notification(); refresh(); }

static void screen_deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *txt, int32_t x, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_size(b, 148, 40);
    lv_obj_set_pos(b, x, 192);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

void ui_show_ble(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, screen_deleted_cb, LV_EVENT_DELETE, NULL);
    ui_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28_deg, 0);
    lv_label_set_text(title, LV_SYMBOL_BLUETOOTH " Bluetooth");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 40, 10);

    state_label = lv_label_create(scr);
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(state_label, 12, 52);

    lv_obj_t *hint = lv_label_create(scr);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x909090), 0);
    char name[16], txt[64];
    ble_ui_device_name(name, sizeof name);
    lv_snprintf(txt, sizeof txt, "Name: %s   (ST BLE Toolbox -> P2P Server)", name);
    lv_label_set_text(hint, txt);
    lv_obj_set_pos(hint, 12, 80);

    led = lv_led_create(scr);
    lv_obj_set_size(led, 30, 30);
    lv_obj_set_pos(led, 16, 110);
    lv_led_set_color(led, lv_color_hex(0x2080FF));        /* blue, like LD1 in ST's demo */

    led_label = lv_label_create(scr);
    lv_obj_set_style_text_font(led_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(led_label, 60, 114);

    temp_label = lv_label_create(scr);
    lv_obj_set_style_text_font(temp_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(temp_label, lv_color_hex(0xFF8040), 0);
    lv_obj_set_pos(temp_label, 12, 142);

    notif_label = lv_label_create(scr);
    lv_obj_set_style_text_font(notif_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(notif_label, lv_color_hex(0x00D0FF), 0);
    lv_obj_set_pos(notif_label, 12, 164);

    lv_obj_t *adv = make_button(scr, "Advertise", 8, adv_cb);
    make_button(scr, "Notify phone", 164, notify_cb);

    refresh();
    refresh_timer = lv_timer_create(refresh_timer_cb, 250, NULL);
    ui_load(scr, adv);
}
