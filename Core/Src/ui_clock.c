/* Desk clock (menu 13) – framework for the 4 selectable layouts.
 * [< MENU] [STYLE] top-left on every layout. STYLE = next layout.
 * Tapping the time = 12 h / 24 h. Both choices are saved in flash (settings.c). */
#include "main.h"
#include "ui.h"
#include "ui_clock.h"
#include "settings.h"
#include <string.h>

static const clock_layout_t *const LAYOUTS[] = { &CLOCK_HUD, &CLOCK_TERM, &CLOCK_RADAR, &CLOCK_FLIP };
#define N_LAYOUTS (sizeof LAYOUTS / sizeof LAYOUTS[0])

static uint8_t     style, h24, loaded;
static uint8_t     last_min = 0xFF;
static lv_timer_t *timer;

/* ---------------- helpers for the layouts ---------------- */
lv_obj_t *clock_box(lv_obj_t *p, int x, int y, int w, int h, uint32_t col)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 2, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
    return o;
}

lv_obj_t *clock_label(lv_obj_t *p, const lv_font_t *f, uint32_t col, int spacing)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_text_letter_space(l, spacing, 0);
    lv_label_set_text(l, "");
    return l;
}

void clock_text(lv_obj_t *l, char *cache, size_t n, const char *s)
{
    if (strncmp(cache, s, n) == 0) return;     /* unchanged: no redraw (GPIO display is slow) */
    strncpy(cache, s, n - 1);
    cache[n - 1] = '\0';
    lv_label_set_text(l, s);
}

static void tick(void);
static void h24_cb(lv_event_t *e)
{
    (void)e;
    h24 = !h24;
    settings_set_clock(style, h24);
    tick();
}

lv_obj_t *clock_tap_area(lv_obj_t *scr, int x, int y, int w, int h)
{
    lv_obj_t *b = lv_button_create(scr);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_add_event_cb(b, h24_cb, LV_EVENT_CLICKED, NULL);
    return b;
}

/* ---------------- data for the layouts ---------------- */
static void tick(void)
{
    clock_data_t d;
    memset(&d, 0, sizeof d);
    d.now = HAL_GetTick();
    d.h24 = h24;
    d.have = phone_time_now(&d.t);
    if (d.have)
    {
        d.hour = d.t.hour;
        d.ampm = "";
        if (!h24) { d.hour = d.t.hour % 12; if (d.hour == 0) d.hour = 12; d.ampm = d.t.hour < 12 ? "AM" : "PM"; }
        d.minute_changed = (last_min != 0xFF && d.t.minute != last_min);
        last_min = d.t.minute;
    }
    else d.ampm = "";
    presence_get_info(&d.in);
    d.dbm = d.in.filt10 / 10;
    d.bat = phone_battery();
    d.sync_ago = phone_time_synced_ago_s();
    LAYOUTS[style]->update(&d);
}

static void timer_cb(lv_timer_t *t) { (void)t; tick(); }

/* ---------------- buttons ---------------- */
static void back_cb(lv_event_t *e) { (void)e; ui_go(ui_show_home); }

static void style_cb(lv_event_t *e)
{
    (void)e;
    style = (uint8_t)((style + 1) % N_LAYOUTS);
    settings_set_clock(style, h24);
    ui_go(ui_show_clock);                       /* rebuild with the next layout */
}

static lv_obj_t *hud_button(lv_obj_t *scr, int x, const char *txt, uint32_t col, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(scr);
    lv_obj_set_pos(b, x, 4);
    lv_obj_set_size(b, 62, 22);
    lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(col), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_radius(b, 2, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = clock_label(b, &lv_font_montserrat_14, col, 1);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    return b;
}

static void deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(timer);
    timer = NULL;
}

void ui_show_clock(void)
{
    if (!loaded) { settings_get_clock(&style, &h24); loaded = 1; }
    if (style >= N_LAYOUTS) style = 0;
    const clock_layout_t *L = LAYOUTS[style];

    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(L->bg), 0);
    lv_obj_add_event_cb(scr, deleted_cb, LV_EVENT_DELETE, NULL);

    L->create(scr);
    lv_obj_t *back = hud_button(scr, 4, "< MENU", L->accent, back_cb);
    hud_button(scr, 70, "STYLE", L->accent, style_cb);

    last_min = 0xFF;
    tick();
    timer = lv_timer_create(timer_cb, L->period_ms, NULL);
    ui_load(scr, back);
}
