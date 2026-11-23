/* Desk clock layout F – Flip cards.
 * Four dark cards with big digits (64 px font) and a split line. When a digit
 * changes, a "flap" falls over the top half of its card (short animation). */
#include "main.h"
#include "ui_clock.h"
#include <string.h>

#define C_BG    0x0B0B10
#define C_CARD  0x1A1C26
#define C_FLAP  0x2A2D3C
#define C_TXT   0xF0F0F0
#define C_SUB   0xA0A0B0
#define C_MAG   0xFF2A6D
#define C_DIM   0x5A5A6A

#define CW   60
#define CH   96
#define CTOP 44
static const int16_t CARD_X[4] = { 20, 86, 174, 240 };

static lv_obj_t *card[4], *digit[4], *flap[4], *colon, *date, *line1, *line2;
static char      shown[4], cache[4][40];

static void flap_h(void *o, int32_t v) { lv_obj_set_height((lv_obj_t *)o, v); }

static void flip(int i)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, flap[i]);
    lv_anim_set_exec_cb(&a, flap_h);
    lv_anim_set_values(&a, CH / 2, 0);          /* flap covers the top half, then falls */
    lv_anim_set_duration(&a, 300);
    lv_anim_start(&a);
}

static void set_digit(int i, char c)
{
    if (shown[i] == c) return;
    int animate = shown[i] != 0;                 /* no flip on the first draw */
    shown[i] = c;
    char s[2] = { c, 0 };
    lv_label_set_text(digit[i], s);
    if (animate) flip(i);
}

static void create(lv_obj_t *scr)
{
    for (int i = 0; i < 4; i++)
    {
        card[i] = clock_box(scr, CARD_X[i], CTOP, CW, CH, C_CARD);
        lv_obj_set_style_radius(card[i], 6, 0);
        lv_obj_set_style_clip_corner(card[i], true, 0);
        digit[i] = clock_label(card[i], &lv_font_digits_64, C_TXT, 0);
        lv_obj_center(digit[i]);
        flap[i] = clock_box(card[i], 0, 0, CW, 0, C_FLAP);
        lv_obj_set_style_radius(flap[i], 0, 0);
        clock_box(card[i], 0, CH / 2 - 1, CW, 2, C_BG);         /* split line */
    }
    colon = clock_label(scr, &lv_font_digits_64, C_MAG, 0);
    lv_label_set_text(colon, ":");
    lv_obj_set_pos(colon, 151, CTOP + 10);
    clock_tap_area(scr, 16, CTOP, 288, CH);

    date = clock_label(scr, &lv_font_montserrat_14, C_SUB, 3);
    lv_obj_set_width(date, 320);
    lv_obj_set_style_text_align(date, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(date, 0, 156);
    line1 = clock_label(scr, &lv_font_montserrat_14, C_MAG, 1);
    lv_obj_set_width(line1, 320);
    lv_obj_set_style_text_align(line1, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(line1, 0, 186);
    line2 = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);
    lv_obj_set_width(line2, 320);
    lv_obj_set_style_text_align(line2, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(line2, 0, 210);

    memset(shown, 0, sizeof shown);
    memset(cache, 0, sizeof cache);
}

static void update(const clock_data_t *d)
{
    static const char *const WD[8] = { "", "MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY", "FRIDAY", "SATURDAY", "SUNDAY" };
    static const char *const MO[13] = { "", "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
    char b[40];

    if (d->have)
    {
        set_digit(0, (d->hour / 10 || d->h24) ? (char)('0' + d->hour / 10) : ' ');
        set_digit(1, (char)('0' + d->hour % 10));
        set_digit(2, (char)('0' + d->t.minute / 10));
        set_digit(3, (char)('0' + d->t.minute % 10));
        lv_snprintf(b, sizeof b, "%s  -  %u %s", WD[d->t.weekday], d->t.day, MO[d->t.month]);
        clock_text(date, cache[0], 40, b);
        char bat[8];
        if (d->bat >= 0) lv_snprintf(bat, sizeof bat, "%d%%", d->bat); else lv_snprintf(bat, sizeof bat, "--");
        lv_snprintf(b, sizeof b, "%s  -  %02us  -  iPHONE %s", d->h24 ? "24H" : d->ampm, d->t.second, bat);
        clock_text(line1, cache[1], 40, b);
    }
    else
    {
        for (int i = 0; i < 4; i++) set_digit(i, ' ');
        clock_text(date, cache[0], 40, "WAITING FOR iPHONE TIME");
        clock_text(line1, cache[1], 40, "");
    }
    if (d->in.phone)
        lv_snprintf(b, sizeof b, "%s  %d dBm  -  LAPTOP %s", d->in.paused ? "PAUSED" : (d->in.near ? "NEAR" : "FAR"),
                    d->dbm, d->in.kbd ? "ARMED" : (d->in.laptops ? "LINKED" : "---"));
    else
        lv_snprintf(b, sizeof b, "PHONE AWAY  -  LAPTOP %s", d->in.kbd ? "ARMED" : (d->in.laptops ? "LINKED" : "---"));
    clock_text(line2, cache[2], 40, b);
}

const clock_layout_t CLOCK_FLIP = { "FLIP", C_BG, C_MAG, 250, create, update };
