/* BLE_TFT_AutoLock step T4 – "12. Touch" screen.
 * Shows the raw touch values (to check the panel works), a dot where you touch,
 * and a 3-point calibration: tap the cross top-left, top-right, bottom-left.
 * After calibrating, touch works on every screen (B1 still works too). */
#include "main.h"
#include "ui.h"
#include "touch.h"
#include "settings.h"

#define TX TOUCH_CAL_X                          /* calibration targets (touch.c) */
#define TY TOUCH_CAL_Y

static lv_obj_t   *raw_label, *info_label, *dot, *cross_h, *cross_v;
static lv_timer_t *timer;
static int         cal_step = -1;               /* -1 = not calibrating, 0..2 = waiting for point */
static uint16_t    crx[3], cry[3];
static uint32_t    acc_x, acc_y, acc_n;
static int         was_pressed;

static void show_cross(int i)
{
    lv_obj_set_pos(cross_h, TX[i] - 12, TY[i] - 1);
    lv_obj_set_pos(cross_v, TX[i] - 1, TY[i] - 12);
    lv_obj_set_style_opa(cross_h, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(cross_v, LV_OPA_COVER, 0);
    lv_label_set_text_fmt(info_label, "Tap the cross %d of 3 (press, then lift)", i + 1);
}

static void hide_cross(void)
{
    lv_obj_set_style_opa(cross_h, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(cross_v, LV_OPA_TRANSP, 0);
}

static void timer_cb(lv_timer_t *t)
{
    (void)t;
    uint16_t rx, ry;
    int p = touch_read_raw(&rx, &ry);

    if (p) lv_label_set_text_fmt(raw_label, "raw X %4u   raw Y %4u   PRESSED", rx, ry);
    else   lv_label_set_text(raw_label, "raw X ----   raw Y ----   (not pressed)");

    int16_t sx, sy;
    if (p && cal_step < 0 && touch_map(rx, ry, &sx, &sy))
    {
        lv_obj_set_pos(dot, sx - 5, sy - 5);
        lv_obj_set_style_opa(dot, LV_OPA_COVER, 0);
    }

    if (cal_step >= 0)
    {
        if (p) { acc_x += rx; acc_y += ry; acc_n++; }
        else if (was_pressed && acc_n >= 2)            /* lifted: store this point */
        {
            crx[cal_step] = (uint16_t)(acc_x / acc_n);
            cry[cal_step] = (uint16_t)(acc_y / acc_n);
            acc_x = acc_y = acc_n = 0;
            if (++cal_step < 3) show_cross(cal_step);
            else
            {
                touch_set_calibration(crx, cry, TX, TY);
                cal_step = -1;
                hide_cross();
                if (touch_is_calibrated()) settings_save_touch_cal(crx, cry);   /* keep after power-off */
                lv_label_set_text(info_label, touch_is_calibrated()
                    ? "Calibrated + saved! Touch works on all screens."
                    : "Calibration failed - try again");
            }
        }
    }
    was_pressed = p;
}

static void calib_cb(lv_event_t *e)
{
    (void)e;
    cal_step = 0;
    acc_x = acc_y = acc_n = 0;
    was_pressed = 1;                   /* ignore the press that started calibration */
    lv_obj_set_style_opa(dot, LV_OPA_TRANSP, 0);
    show_cross(0);
}

static void deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(timer);
    timer = NULL;
    cal_step = -1;
}

static lv_obj_t *line_obj(lv_obj_t *scr, int w, int h)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(0xFF3030), 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_opa(o, LV_OPA_TRANSP, 0);
    return o;
}

void ui_show_touch(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *back = ui_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "Touch");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 110, 36);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_event_cb(btn, calib_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_label_set_text(bl, "Calibrate");
    lv_obj_center(bl);

    raw_label = lv_label_create(scr);
    lv_obj_set_style_text_font(raw_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(raw_label, lv_color_hex(0x00D0FF), 0);
    lv_obj_align(raw_label, LV_ALIGN_TOP_MID, 0, 60);

    info_label = lv_label_create(scr);
    lv_obj_set_style_text_font(info_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(info_label, lv_color_hex(0xFFD000), 0);
    lv_obj_align(info_label, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_label_set_text(info_label, touch_is_calibrated() ? "Touch anywhere: the dot follows"
                                                        : "Press the screen: raw values change. Then Calibrate.");

    dot = lv_obj_create(scr);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, 5, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x30FF30), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_opa(dot, LV_OPA_TRANSP, 0);

    cross_h = line_obj(scr, 24, 3);
    cross_v = line_obj(scr, 3, 24);

    cal_step = -1;
    timer = lv_timer_create(timer_cb, 50, NULL);
    ui_load(scr, back);
}
