/* Step 5.3 – "Phone message" screen: shows the last text written by the phone. */
#include "main.h"
#include "ui.h"
#include "ble_text_service.h"

static lv_obj_t   *msg_label, *info_label;
static lv_timer_t *refresh_timer;
static uint32_t    shown_count = 0xFFFFFFFF;

static void refresh(void)
{
    uint32_t n = ble_text_count();
    if (n != shown_count) {                         /* new message: update the big text */
        shown_count = n;
        lv_label_set_text(msg_label, n ? ble_text_last() : "(no message yet)");
    }
    if (n) {
        uint32_t s = (HAL_GetTick() - ble_text_time_ms()) / 1000U;
        lv_label_set_text_fmt(info_label, "%lu message(s), last %lu s ago",
                              (unsigned long)n, (unsigned long)s);
    } else {
        lv_label_set_text(info_label, "Write text to service A07E0001-... with your phone");
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void screen_deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
}

void ui_show_message(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, screen_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *back = ui_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28_deg, 0);
    lv_label_set_text(title, "Message");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 40, 10);

    /* framed box with the text, wraps over several lines */
    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_set_size(box, 304, 150);
    lv_obj_align(box, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x00D0FF), 0);
    lv_obj_set_style_border_width(box, 2, 0);

    msg_label = lv_label_create(box);
    lv_obj_set_width(msg_label, 280);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(msg_label, &lv_font_montserrat_28_deg, 0);
    lv_obj_set_style_text_color(msg_label, lv_color_white(), 0);
    lv_obj_align(msg_label, LV_ALIGN_TOP_LEFT, 0, 0);

    info_label = lv_label_create(scr);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0x909090), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_LEFT, 10, -12);

    shown_count = 0xFFFFFFFF;
    refresh();
    refresh_timer = lv_timer_create(refresh_timer_cb, 250, NULL);
    ui_load(scr, back);
}
