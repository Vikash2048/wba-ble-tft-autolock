/* Desk clock layout E – Radar.
 * Rings + rotating sweep line. The iPhone is a magenta blip whose distance from
 * the centre follows its signal strength (strong = close). Each time the sweep
 * passes a blip it lights up and fades ("ping"). Time and status on the right. */
#include "main.h"
#include "ui_clock.h"
#include <string.h>

#define C_BG    0x05070F
#define C_CYAN  0x00F0FF
#define C_TRAIL 0x006878
#define C_MAG   0xFF2A6D
#define C_DIM   0x3A6A80
#define C_GRID  0x0E2A38
#define C_YEL   0xFFD000

#define CX  106
#define CY  132
#define R   96

/* sin(k * 10 deg) * 1000, k = 0..35 (no float maths needed) */
static const int16_t SIN10[36] = {
    0, 174, 342, 500, 643, 766, 866, 940, 985, 1000, 985, 940, 866, 766, 643, 500, 342, 174,
    0, -174, -342, -500, -643, -766, -866, -940, -985, -1000, -985, -940, -866, -766, -643, -500, -342, -174 };
#define SINK(k) SIN10[((k) % 36 + 36) % 36]
#define COSK(k) SINK((k) + 9)

#define PHONE_K   4      /* blip directions: 40 deg and 220 deg (0 = up, clockwise) */
#define LAPTOP_K  22

static lv_obj_t *sweep, *trail, *phone, *phone_lbl, *laptop, *laptop_lbl;
static lv_obj_t *tm, *sub, *date, *st, *l_dbm, *l_pwr, *l_host;
static lv_point_precise_t sp[2], tp[2];
static int      k, ping_phone, ping_laptop, c_phone_lvl, c_laptop_lvl, c_r, c_laptop_vis, c_st_col;
static char     cache[8][32];

static const uint32_t PING_COL[4] = { 0x401428, 0x80203F, 0xC02050, C_MAG };

static void ring(lv_obj_t *scr, int r)
{
    lv_obj_t *o = lv_obj_create(scr);
    lv_obj_set_size(o, 2 * r, 2 * r);
    lv_obj_set_pos(o, CX - r, CY - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(C_GRID), 0);
    lv_obj_set_style_border_width(o, 1, 0);
}

static lv_obj_t *dot(lv_obj_t *scr, int size, uint32_t col)
{
    lv_obj_t *o = clock_box(scr, 0, 0, size, size, col);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    return o;
}

static lv_obj_t *line(lv_obj_t *scr, uint32_t col, int w)
{
    lv_obj_t *l = lv_line_create(scr);
    lv_obj_set_style_line_color(l, lv_color_hex(col), 0);
    lv_obj_set_style_line_width(l, w, 0);
    lv_obj_set_style_line_rounded(l, true, 0);
    return l;
}

static void place(lv_obj_t *o, lv_obj_t *lbl, int kk, int r, int size)
{
    int x = CX + (r * SINK(kk)) / 1000, y = CY - (r * COSK(kk)) / 1000;
    lv_obj_set_pos(o, x - size / 2, y - size / 2);
    if (lbl) lv_obj_set_pos(lbl, x + 8, y - 16);
}

static void create(lv_obj_t *scr)
{
    ring(scr, R); ring(scr, 64); ring(scr, 32);
    clock_box(scr, CX - R, CY, 2 * R, 1, C_GRID);           /* cross hair */
    clock_box(scr, CX, CY - R, 1, 2 * R, C_GRID);

    trail = line(scr, C_TRAIL, 2);
    sweep = line(scr, C_CYAN, 2);
    sp[0].x = tp[0].x = CX; sp[0].y = tp[0].y = CY;

    laptop = dot(scr, 8, C_DIM);
    laptop_lbl = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);
    lv_label_set_text(laptop_lbl, "LAPTOP");
    place(laptop, laptop_lbl, LAPTOP_K, 60, 8);
    phone = dot(scr, 10, PING_COL[0]);
    phone_lbl = clock_label(scr, &lv_font_montserrat_14, C_MAG, 1);
    lv_label_set_text(phone_lbl, "iPHONE");
    lv_obj_t *c = dot(scr, 8, C_CYAN);
    lv_obj_set_pos(c, CX - 4, CY - 4);

    tm = clock_label(scr, &lv_font_montserrat_28_deg, C_CYAN, 1);
    lv_obj_set_pos(tm, 214, 32);
    clock_tap_area(scr, 210, 30, 106, 54);
    sub  = clock_label(scr, &lv_font_montserrat_14, C_MAG, 1);  lv_obj_set_pos(sub, 216, 64);
    date = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);  lv_obj_set_pos(date, 216, 84);
    st     = clock_label(scr, &lv_font_montserrat_20, C_CYAN, 2); lv_obj_set_pos(st, 216, 146);
    l_dbm  = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);  lv_obj_set_pos(l_dbm, 216, 174);
    l_pwr  = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);  lv_obj_set_pos(l_pwr, 216, 192);
    l_host = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);  lv_obj_set_pos(l_host, 216, 210);

    memset(cache, 0, sizeof cache);
    k = 0; ping_phone = ping_laptop = 0;
    c_phone_lvl = c_laptop_lvl = -1; c_r = -1; c_laptop_vis = -1; c_st_col = -1;
}

static void update(const clock_data_t *d)
{
    static const char *const WD[8] = { "", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
    static const char *const MO[13] = { "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    char b[32];

    /* sweep: +10 deg per update, trail 10 deg behind */
    k = (k + 1) % 36;
    sp[1].x = CX + (R * SINK(k)) / 1000;     sp[1].y = CY - (R * COSK(k)) / 1000;
    tp[1].x = CX + (R * SINK(k - 1)) / 1000; tp[1].y = CY - (R * COSK(k - 1)) / 1000;
    lv_line_set_points(trail, tp, 2);
    lv_line_set_points(sweep, sp, 2);

    /* phone blip: distance from signal (-40 dBm = 14 px ... -90 dBm = 92 px) */
    if (d->in.phone)
    {
        int r = 14 + ((-40 - LV_CLAMP(-90, d->dbm, -40)) * 78) / 50;
        if (r != c_r) { c_r = r; place(phone, phone_lbl, PHONE_K, r, 10); }
        if (k == PHONE_K) ping_phone = 3;
    }
    int lvl = d->in.phone ? ping_phone : 0;
    if (ping_phone) ping_phone--;
    if (lvl != c_phone_lvl)
    {
        c_phone_lvl = lvl;
        lv_obj_set_style_bg_color(phone, lv_color_hex(PING_COL[lvl]), 0);
        lv_obj_set_style_text_opa(phone_lbl, d->in.phone ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        if (!d->in.phone) lv_obj_set_style_bg_opa(phone, LV_OPA_TRANSP, 0);
        else              lv_obj_set_style_bg_opa(phone, LV_OPA_COVER, 0);
    }

    int vis = d->in.laptops > 0;
    if (vis && k == LAPTOP_K) ping_laptop = 3;
    int ll = vis ? (ping_laptop ? 1 : 0) : -2;
    if (ping_laptop) ping_laptop--;
    if (ll != c_laptop_lvl)
    {
        c_laptop_lvl = ll;
        lv_obj_set_style_bg_opa(laptop, vis ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_bg_color(laptop, lv_color_hex(ll == 1 ? C_CYAN : C_DIM), 0);
        lv_obj_set_style_text_opa(laptop_lbl, vis ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }

    /* right panel */
    if (d->have)
    {
        lv_snprintf(b, sizeof b, "%02u:%02u", d->hour, d->t.minute);                 clock_text(tm, cache[0], 32, b);
        lv_snprintf(b, sizeof b, "%s %02us", d->h24 ? "24H" : d->ampm, d->t.second);   clock_text(sub, cache[1], 32, b);
        lv_snprintf(b, sizeof b, "%s %u %s", WD[d->t.weekday], d->t.day, MO[d->t.month]); clock_text(date, cache[2], 32, b);
    }
    else
    {
        clock_text(tm, cache[0], 32, "--:--");
        clock_text(sub, cache[1], 32, "NO TIME");
        clock_text(date, cache[2], 32, "");
    }
    const char *s = !d->in.phone ? "SCAN" : d->in.paused ? "PAUSE" : (d->in.near ? "NEAR" : "FAR");
    clock_text(st, cache[3], 32, s);
    int sc = !d->in.phone ? 0 : d->in.paused ? 1 : (d->in.near ? 2 : 3);
    if (sc != c_st_col)
    {
        static const uint32_t COL[4] = { C_DIM, C_YEL, C_CYAN, C_MAG };
        c_st_col = sc;
        lv_obj_set_style_text_color(st, lv_color_hex(COL[sc]), 0);
    }
    if (d->in.phone) lv_snprintf(b, sizeof b, "%d dBm", d->dbm); else lv_snprintf(b, sizeof b, "--- dBm");
    clock_text(l_dbm, cache[4], 32, b);
    if (d->bat >= 0) lv_snprintf(b, sizeof b, "PWR %d%%", d->bat); else lv_snprintf(b, sizeof b, "PWR --");
    clock_text(l_pwr, cache[5], 32, b);
    clock_text(l_host, cache[6], 32, d->in.kbd ? "HOST ARMED" : (d->in.laptops ? "HOST LINKED" : "HOST ---"));
}

const clock_layout_t CLOCK_RADAR = { "RADAR", C_BG, C_CYAN, 150, create, update };
