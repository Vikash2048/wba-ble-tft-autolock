/* BLE_TFT_AutoLock step C – "11. AutoLock" screen (the log stays in "10. AutoLock log").
 *
 *  ┌──────────────────────────────────────────┐
 *  │ < Menu          AutoLock                 │
 *  │ ┌──────────┐  -58.3 dBm                  │
 *  │ │   NEAR   │  Phone:  OK                 │
 *  │ └──────────┘  Laptop: ready               │
 *  │               LOCK in 1500 ms             │
 *  │ ~~~~~~~ signal, last 30 s ~~~~~~~~~~~~~~ │  green line = NEAR level, red = FAR level
 *  │ NEAR > -54   FAR < -62   wait 5 s        │
 *  │ [ Pause ]                [ Forget pairs ]│
 *  └──────────────────────────────────────────┘
 * B1: tap = next button, hold = press. "Forget pairings" asks "Sure?" first. */
#include "main.h"
#include "ui.h"
#include "presence.h"
#include "settings.h"

#define HISTORY   60          /* 60 x 0.5 s = 30 s */
#define Y_MIN   (-100)
#define Y_MAX   (-30)

static lv_obj_t   *state_box, *state_label, *dbm_label, *phone_label, *laptop_label, *pend_label;
static lv_obj_t   *chart, *set_label, *pause_label, *forget_btn, *forget_label;
static lv_chart_series_t *s_rssi, *s_near, *s_far;
static lv_timer_t *timer;
static uint32_t    confirm_until;     /* "Sure?" is armed until this tick */

static int32_t clamp(int32_t v) { return v < Y_MIN ? Y_MIN : (v > Y_MAX ? Y_MAX : v); }

static void refresh(void)
{
    presence_info_t in;
    presence_get_info(&in);
    const settings_t *st = settings_get();

    /* big state */
    const char *txt; uint32_t col;
    if (!in.phone)      { txt = "NO PHONE"; col = 0x505050; }
    else if (in.paused) { txt = "PAUSED";   col = 0xC09000; }
    else if (in.near)   { txt = "NEAR";     col = 0x108020; }
    else                { txt = "FAR";      col = 0xB01818; }
    lv_label_set_text(state_label, txt);
    lv_obj_set_style_bg_color(state_box, lv_color_hex(col), 0);

    int f = in.filt10, af = f < 0 ? -f : f;
    if (in.phone) lv_label_set_text_fmt(dbm_label, "%s%d.%d dBm", f < 0 ? "-" : "", af / 10, af % 10);
    else          lv_label_set_text(dbm_label, "-- dBm");

    lv_label_set_text_fmt(phone_label, "Phone:  %s", in.phone ? "OK" : "--");
    lv_label_set_text_fmt(laptop_label, "Laptop: %s",
                          in.kbd ? "ready" : (in.laptops ? "connected" : "--"));
    if (in.cond_ms) lv_label_set_text_fmt(pend_label, "%s in %lu ms",
                                          in.near ? "LOCK" : "NEAR",
                                          (unsigned long)(((in.near ? st->far_hold_s : st->near_hold_s) * 1000U > in.cond_ms)
                                              ? (in.near ? st->far_hold_s : st->near_hold_s) * 1000U - in.cond_ms : 0));
    else            lv_label_set_text(pend_label, "");

    lv_label_set_text_fmt(set_label, "NEAR > %d   FAR < %d   wait %d s",
                          st->near_dbm, st->far_dbm, st->far_hold_s);
    lv_label_set_text(pause_label, in.paused ? "Resume" : "Pause");

    /* graph: smoothed RSSI + the two threshold lines */
    lv_chart_set_next_value(chart, s_rssi, in.phone ? clamp(f / 10) : LV_CHART_POINT_NONE);
    lv_chart_set_next_value(chart, s_near, clamp(st->near_dbm));
    lv_chart_set_next_value(chart, s_far,  clamp(st->far_dbm));

    /* "Sure?" times out after 5 s */
    if (confirm_until && (int32_t)(HAL_GetTick() - confirm_until) >= 0)
    {
        confirm_until = 0;
        lv_label_set_text(forget_label, "Forget pairings");
        lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0x404040), 0);
    }
}

static void timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void pause_cb(lv_event_t *e) { (void)e; presence_toggle_pause(); }

static void forget_cb(lv_event_t *e)
{
    (void)e;
    if (!confirm_until)                         /* first press: ask */
    {
        confirm_until = HAL_GetTick() + 5000U;
        lv_label_set_text(forget_label, "Sure? press again");
        lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0xB01818), 0);
    }
    else                                        /* second press within 5 s: do it */
    {
        confirm_until = 0;
        presence_forget_all();
        lv_label_set_text(forget_label, "Forgotten");
        lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0x404040), 0);
    }
}

static void deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(timer);
    timer = NULL;
}

static lv_obj_t *small_label(lv_obj_t *scr, int x, int y, uint32_t color)
{
    lv_obj_t *l = lv_label_create(scr);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

void ui_show_autolock_view(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *back = ui_add_back_button(scr);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_label_set_text(title, "AutoLock");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 30, 12);

    /* big NEAR / FAR box */
    state_box = lv_obj_create(scr);
    lv_obj_set_size(state_box, 144, 58);
    lv_obj_set_pos(state_box, 8, 48);
    lv_obj_set_style_border_width(state_box, 0, 0);
    lv_obj_set_style_radius(state_box, 8, 0);
    state_label = lv_label_create(state_box);
    lv_obj_set_style_text_font(state_label, &lv_font_montserrat_28_deg, 0);
    lv_obj_set_style_text_color(state_label, lv_color_white(), 0);
    lv_obj_center(state_label);

    dbm_label = lv_label_create(scr);
    lv_obj_set_style_text_font(dbm_label, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(dbm_label, 162, 44);
    phone_label  = small_label(scr, 162, 68, 0x00D0FF);    /* one line each - */
    laptop_label = small_label(scr, 162, 84, 0x00D0FF);    /* side by side was cut off */
    pend_label   = small_label(scr, 162, 100, 0xFFD000);

    /* signal history */
    chart = lv_chart_create(scr);
    lv_obj_set_size(chart, 304, 60);
    lv_obj_set_pos(chart, 8, 120);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, HISTORY);
    lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(chart, LV_CHART_AXIS_PRIMARY_Y, Y_MIN, Y_MAX);
    lv_chart_set_div_line_count(chart, 3, 0);
    lv_obj_set_style_bg_color(chart, lv_color_hex(0x101010), 0);
    lv_obj_set_style_line_color(chart, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(chart, 2, 0);
    s_near = lv_chart_add_series(chart, lv_color_hex(0x206030), LV_CHART_AXIS_PRIMARY_Y);
    s_far  = lv_chart_add_series(chart, lv_color_hex(0x702020), LV_CHART_AXIS_PRIMARY_Y);
    s_rssi = lv_chart_add_series(chart, lv_color_hex(0xFFFFFF), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(chart, s_rssi, LV_CHART_POINT_NONE);
    lv_chart_set_all_values(chart, s_near, LV_CHART_POINT_NONE);
    lv_chart_set_all_values(chart, s_far,  LV_CHART_POINT_NONE);

    set_label = small_label(scr, 10, 183, 0x909090);

    /* buttons */
    lv_obj_t *pause = lv_button_create(scr);
    lv_obj_set_size(pause, 100, 36);
    lv_obj_align(pause, LV_ALIGN_BOTTOM_LEFT, 8, -4);
    lv_obj_add_event_cb(pause, pause_cb, LV_EVENT_CLICKED, NULL);
    pause_label = lv_label_create(pause);
    lv_obj_set_style_text_font(pause_label, &lv_font_montserrat_14, 0);
    lv_obj_center(pause_label);

    forget_btn = lv_button_create(scr);
    lv_obj_set_size(forget_btn, 160, 36);
    lv_obj_align(forget_btn, LV_ALIGN_BOTTOM_RIGHT, -8, -4);
    lv_obj_set_style_bg_color(forget_btn, lv_color_hex(0x404040), 0);
    lv_obj_add_event_cb(forget_btn, forget_cb, LV_EVENT_CLICKED, NULL);
    forget_label = lv_label_create(forget_btn);
    lv_obj_set_style_text_font(forget_label, &lv_font_montserrat_14, 0);
    lv_label_set_text(forget_label, "Forget pairings");
    lv_obj_center(forget_label);

    confirm_until = 0;
    refresh();
    timer = lv_timer_create(timer_cb, 500, NULL);    /* same rate as the RSSI readings */
    ui_load(scr, back);
}
