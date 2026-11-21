/* BLE_TFT_AutoLock – "10. AutoLock log" screen.
 * Top: live status (phone RSSI, NEAR/FAR, laptop keyboard, paused).
 * Below: the newest log lines (screen_log.c) – replaces the PC serial log,
 * which can't work here because UART TX (PA6) is the display's D7 wire. */
#include "main.h"
#include "ui.h"
#include "screen_log.h"
#include "presence.h"

#define LOG_LINES_SHOWN  9    /* entries; long ones wrap to 2 lines, oldest scroll off the top */

static lv_obj_t   *status1, *status2, *log_label, *pause_label;
static lv_timer_t *refresh_timer;
static uint32_t    shown_count;
static char        logbuf[LOG_LINES_SHOWN * 64];

static void refresh(void)
{
    presence_info_t in;
    presence_get_info(&in);

    int f = in.filt10, af = f < 0 ? -f : f;
    if (in.phone)
    {
        lv_label_set_text_fmt(status1, "Phone %s%d.%d dBm  %s%s",
                              f < 0 ? "-" : "", af / 10, af % 10,
                              in.near ? "NEAR" : "FAR",
                              in.paused ? "  PAUSED" : "");
        lv_obj_set_style_text_color(status1, in.paused ? lv_color_hex(0xFFD000)
                                     : in.near ? lv_color_hex(0x30E030) : lv_color_hex(0xFF4040), 0);
    }
    else
    {
        lv_label_set_text(status1, "Phone: not connected");
        lv_obj_set_style_text_color(status1, lv_color_hex(0x909090), 0);
    }
    lv_label_set_text_fmt(status2, "Laptop: %s   pending %lu ms",
                          in.kbd ? "keyboard ready" : (in.laptops ? "connected" : "--"),
                          (unsigned long)in.cond_ms);
    lv_label_set_text(pause_label, in.paused ? "Resume" : "Pause");

    if (slog_count() != shown_count)
    {
        shown_count = slog_count();
        slog_get(logbuf, sizeof logbuf, LOG_LINES_SHOWN);
        lv_label_set_text(log_label, shown_count ? logbuf : "(no log yet)");
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void pause_cb(lv_event_t *e) { (void)e; presence_toggle_pause(); refresh(); }

static void screen_deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
}

void ui_show_autolock(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, screen_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *back = ui_add_back_button(scr);

    /* Pause / Resume (B1: tap = move here, hold = press) */
    lv_obj_t *pause = lv_button_create(scr);
    lv_obj_set_size(pause, 84, 36);
    lv_obj_align(pause, LV_ALIGN_TOP_RIGHT, -6, 6);
    lv_obj_add_event_cb(pause, pause_cb, LV_EVENT_CLICKED, NULL);
    pause_label = lv_label_create(pause);
    lv_obj_set_style_text_font(pause_label, &lv_font_montserrat_14, 0);
    lv_obj_center(pause_label);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "AutoLock");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    status1 = lv_label_create(scr);
    lv_obj_set_style_text_font(status1, &lv_font_montserrat_14, 0);
    lv_obj_align(status1, LV_ALIGN_TOP_LEFT, 8, 46);

    status2 = lv_label_create(scr);
    lv_obj_set_style_text_font(status2, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status2, lv_color_hex(0x00D0FF), 0);
    lv_obj_align(status2, LV_ALIGN_TOP_LEFT, 8, 64);

    /* log box */
    lv_obj_t *box = lv_obj_create(scr);
    lv_obj_set_size(box, 312, 150);
    lv_obj_align(box, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x101820), 0);
    lv_obj_set_style_border_color(box, lv_color_hex(0x404850), 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_pad_all(box, 3, 0);

    log_label = lv_label_create(box);
    lv_obj_set_width(log_label, 304);
    lv_label_set_long_mode(log_label, LV_LABEL_LONG_WRAP);   /* long lines continue on the next line */
    lv_obj_set_style_text_font(log_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(log_label, lv_color_hex(0xD0D0D0), 0);
    lv_obj_align(log_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    shown_count = 0xFFFFFFFF;
    refresh();
    refresh_timer = lv_timer_create(refresh_timer_cb, 300, NULL);
    ui_load(scr, back);
}
