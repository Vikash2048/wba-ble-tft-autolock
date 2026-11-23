/* Desk clock layout A – Cyber HUD.
 * Neon 7-segment digits (LVGL shadow = glow), cyan/magenta, HUD corner brackets,
 * seconds sweep bar, signal bars, proximity / laptop / battery, signal sparkline.
 * New minute -> digits flash magenta (glitch). */
#include "main.h"
#include "ui_clock.h"
#include <string.h>

#define C_BG      0x02040A
#define C_CYAN    0x00F0FF
#define C_SEG_OFF 0x08161E
#define C_MAG     0xFF2A6D
#define C_DIM     0x3A6A80
#define C_GRID    0x0E2A38
#define C_YEL     0xFFD000

#define DW   46
#define DH   90
#define T    9
#define TOP  40
static const int16_t DIGIT_X[4] = { 16, 70, 140, 194 };
#define COLON_X 122
static const uint8_t SEG[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

static lv_obj_t *seg[4][7], *colon[2], *sig[4];
static lv_obj_t *title, *ampm, *secs, *sweep, *date, *l_prox, *l_rssi, *l_host, *bat_bar, *bat_txt, *spark;
static lv_chart_series_t *spark_s;
static uint8_t  shown_bits[4], shown_glitch[4], glitch;
static uint32_t n_upd, c_prox_col;
static int      c_colon, c_bars, c_bat;
static char     cache[8][32];

static lv_obj_t *seg_box(lv_obj_t *p, int x, int y, int w, int h)
{
    lv_obj_t *o = clock_box(p, x, y, w, h, C_SEG_OFF);
    lv_obj_set_style_shadow_opa(o, LV_OPA_50, 0);
    return o;
}

static void seg_on(lv_obj_t *o, int on, uint32_t col)
{
    lv_obj_set_style_bg_color(o, lv_color_hex(on ? col : C_SEG_OFF), 0);
    lv_obj_set_style_shadow_width(o, on ? 10 : 0, 0);          /* the neon glow */
    lv_obj_set_style_shadow_color(o, lv_color_hex(col), 0);
}

static void set_digit(int i, int v, int g)                       /* v: 0..9, -1 blank, 10 '-' */
{
    uint8_t bits = v < 0 ? 0 : (v == 10 ? 0x40 : SEG[v]);
    if (shown_bits[i] == bits && shown_glitch[i] == g) return;
    shown_bits[i] = bits; shown_glitch[i] = (uint8_t)g;
    for (int s = 0; s < 7; s++) seg_on(seg[i][s], (bits >> s) & 1, g ? C_MAG : C_CYAN);
}

static void corner(lv_obj_t *p, int x, int y, lv_border_side_t sides)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, 14, 14);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(C_CYAN), 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_side(o, sides, 0);
}

static void create(lv_obj_t *scr)
{
    title = clock_label(scr, &lv_font_montserrat_14, C_DIM, 2);
    lv_obj_set_pos(title, 142, 7);
    lv_obj_t *lk = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);
    lv_label_set_text(lk, "LINK");
    lv_obj_set_pos(lk, 246, 7);
    for (int i = 0; i < 4; i++) sig[i] = clock_box(scr, 284 + i * 8, 22 - (4 + i * 4), 5, 4 + i * 4, C_GRID);

    corner(scr, 6, 32, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT);
    corner(scr, 300, 32, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_RIGHT);
    corner(scr, 6, 134, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_LEFT);
    corner(scr, 300, 134, LV_BORDER_SIDE_BOTTOM | LV_BORDER_SIDE_RIGHT);

    for (int i = 0; i < 4; i++)
    {
        int x = DIGIT_X[i], y = TOP, half = (DH - 3 * T) / 2;
        seg[i][0] = seg_box(scr, x + T, y,                     DW - 2 * T, T);
        seg[i][1] = seg_box(scr, x + DW - T, y + T,            T, half);
        seg[i][2] = seg_box(scr, x + DW - T, y + 2 * T + half, T, half);
        seg[i][3] = seg_box(scr, x + T, y + DH - T,            DW - 2 * T, T);
        seg[i][4] = seg_box(scr, x, y + 2 * T + half,          T, half);
        seg[i][5] = seg_box(scr, x, y + T,                     T, half);
        seg[i][6] = seg_box(scr, x + T, y + T + half,          DW - 2 * T, T);
    }
    colon[0] = seg_box(scr, COLON_X, TOP + 22, 10, 10);
    colon[1] = seg_box(scr, COLON_X, TOP + DH - 32, 10, 10);
    clock_tap_area(scr, 12, TOP - 4, 236, DH + 8);

    ampm = clock_label(scr, &lv_font_montserrat_14, C_MAG, 2);
    lv_obj_set_pos(ampm, 258, TOP + 2);
    secs = clock_label(scr, &lv_font_montserrat_28_deg, C_MAG, 1);
    lv_obj_set_pos(secs, 256, TOP + DH - 32);

    sweep = lv_bar_create(scr);
    lv_obj_set_pos(sweep, 14, 140);
    lv_obj_set_size(sweep, 292, 3);
    lv_bar_set_range(sweep, 0, 239);
    lv_obj_set_style_radius(sweep, 0, 0);
    lv_obj_set_style_radius(sweep, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sweep, lv_color_hex(C_GRID), 0);
    lv_obj_set_style_bg_color(sweep, lv_color_hex(C_MAG), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(sweep, lv_color_hex(C_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(sweep, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);

    date = clock_label(scr, &lv_font_montserrat_20, C_CYAN, 2);
    lv_obj_set_width(date, 320);
    lv_obj_set_style_text_align(date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(date, 0, 150);

    clock_box(scr, 10, 178, 300, 1, C_GRID);
    l_prox = clock_label(scr, &lv_font_montserrat_14, C_CYAN, 1); lv_obj_set_pos(l_prox, 12, 184);
    l_rssi = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);  lv_obj_set_pos(l_rssi, 12, 202);
    l_host = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);  lv_obj_set_pos(l_host, 12, 220);

    lv_obj_t *pw = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);
    lv_label_set_text(pw, "PWR");
    lv_obj_set_pos(pw, 170, 184);
    bat_bar = lv_bar_create(scr);
    lv_obj_set_pos(bat_bar, 206, 188);
    lv_obj_set_size(bat_bar, 64, 8);
    lv_bar_set_range(bat_bar, 0, 100);
    lv_obj_set_style_radius(bat_bar, 1, 0);
    lv_obj_set_style_radius(bat_bar, 1, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bat_bar, lv_color_hex(C_GRID), 0);
    bat_txt = clock_label(scr, &lv_font_montserrat_14, C_CYAN, 1);
    lv_obj_set_pos(bat_txt, 276, 184);

    spark = lv_chart_create(scr);
    lv_obj_set_pos(spark, 168, 204);
    lv_obj_set_size(spark, 142, 32);
    lv_chart_set_type(spark, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(spark, 60);
    lv_chart_set_update_mode(spark, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_axis_range(spark, LV_CHART_AXIS_PRIMARY_Y, -100, -30);
    lv_chart_set_div_line_count(spark, 0, 0);
    lv_obj_set_style_bg_opa(spark, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spark, 0, 0);
    lv_obj_set_style_pad_all(spark, 1, 0);
    lv_obj_set_style_size(spark, 0, 0, LV_PART_INDICATOR);
    lv_obj_set_style_line_width(spark, 2, LV_PART_ITEMS);
    spark_s = lv_chart_add_series(spark, lv_color_hex(C_CYAN), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_all_values(spark, spark_s, LV_CHART_POINT_NONE);

    memset(cache, 0, sizeof cache);
    memset(shown_bits, 0xFF, sizeof shown_bits);
    memset(shown_glitch, 0, sizeof shown_glitch);
    glitch = 0; n_upd = 0; c_colon = -1; c_bars = -1; c_bat = -2; c_prox_col = 0;
}

static void update(const clock_data_t *d)
{
    static const char *const WD[8] = { "", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
    char b[32];

    if (d->have)
    {
        if (d->minute_changed) glitch = 3;
        int g = glitch > 0;
        if (glitch) glitch--;
        set_digit(0, d->hour / 10 ? d->hour / 10 : (d->h24 ? 0 : -1), g);
        set_digit(1, d->hour % 10, g);
        set_digit(2, d->t.minute / 10, g);
        set_digit(3, d->t.minute % 10, g);
        clock_text(ampm, cache[0], 32, d->h24 ? "24H" : d->ampm);
        lv_snprintf(b, sizeof b, "%02u", d->t.second);                          clock_text(secs, cache[1], 32, b);
        lv_snprintf(b, sizeof b, "%s // %02u.%02u.%u", WD[d->t.weekday], d->t.day, d->t.month, d->t.year);
        clock_text(date, cache[2], 32, b);
        lv_bar_set_value(sweep, d->t.second * 4 + (int)((d->now % 1000U) / 250U), LV_ANIM_OFF);
    }
    else
    {
        for (int i = 0; i < 4; i++) set_digit(i, 10, 0);
        clock_text(ampm, cache[0], 32, "");
        clock_text(secs, cache[1], 32, "--");
        clock_text(date, cache[2], 32, "AWAITING iPHONE TIME");
        lv_bar_set_value(sweep, (int)((d->now / 8U) % 240U), LV_ANIM_OFF);     /* scanning */
    }
    int on = !d->have || ((d->now / 500U) & 1);
    if (on != c_colon) { c_colon = on; seg_on(colon[0], on, C_CYAN); seg_on(colon[1], on, C_CYAN); }

    if (d->sync_ago == 0xFFFFFFFFU) clock_text(title, cache[3], 32, "NO SYNC");
    else if (d->sync_ago < 120)     clock_text(title, cache[3], 32, "SYNC OK");
    else { lv_snprintf(b, sizeof b, "SYNC %lum", (unsigned long)(d->sync_ago / 60U)); clock_text(title, cache[3], 32, b); }

    int bars = !d->in.phone ? 0 : d->dbm > -55 ? 4 : d->dbm > -65 ? 3 : d->dbm > -75 ? 2 : 1;
    if (bars != c_bars)
    {
        c_bars = bars;
        for (int i = 0; i < 4; i++) lv_obj_set_style_bg_color(sig[i], lv_color_hex(i < bars ? C_CYAN : C_GRID), 0);
    }

    const char *st = !d->in.phone ? "OFFLINE" : d->in.paused ? "PAUSED" : (d->in.near ? "NEAR" : "FAR");
    lv_snprintf(b, sizeof b, "PROX  %s", st);                                   clock_text(l_prox, cache[4], 32, b);
    uint32_t pc = !d->in.phone ? C_DIM : d->in.paused ? C_YEL : (d->in.near ? C_CYAN : C_MAG);
    if (pc != c_prox_col) { c_prox_col = pc; lv_obj_set_style_text_color(l_prox, lv_color_hex(pc), 0); }
    if (d->in.phone) lv_snprintf(b, sizeof b, "RSSI  %d dBm", d->dbm); else lv_snprintf(b, sizeof b, "RSSI  ---");
    clock_text(l_rssi, cache[5], 32, b);
    clock_text(l_host, cache[6], 32, d->in.kbd ? "HOST  ARMED" : (d->in.laptops ? "HOST  LINKED" : "HOST  ---"));

    if (d->bat != c_bat)
    {
        c_bat = d->bat;
        lv_bar_set_value(bat_bar, d->bat < 0 ? 0 : d->bat, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(bat_bar, lv_color_hex(d->bat >= 0 && d->bat < 20 ? C_MAG : C_CYAN), LV_PART_INDICATOR);
        if (d->bat >= 0) lv_snprintf(b, sizeof b, "%d%%", d->bat); else lv_snprintf(b, sizeof b, "--");
        clock_text(bat_txt, cache[7], 32, b);
    }

    if ((n_upd++ & 3) == 0)
        lv_chart_set_next_value(spark, spark_s, d->in.phone ? LV_CLAMP(-100, d->dbm, -30) : LV_CHART_POINT_NONE);
}

const clock_layout_t CLOCK_HUD = { "HUD", C_BG, C_CYAN, 250, create, update };
