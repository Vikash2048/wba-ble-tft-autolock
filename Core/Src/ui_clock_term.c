/* Desk clock layout D – Retro terminal.
 * Green phosphor text, a "boot" intro (prompt types itself, lines appear one by one),
 * blinking block cursor. Text only -> very light on RAM and redraw time. */
#include "main.h"
#include "ui_clock.h"
#include <string.h>

#define C_BG    0x020A04
#define C_GRN   0x33FF66
#define C_DIM   0x1A7F33
#define C_RED   0xFF5555

#define N_ROWS  5
static const char *const KEYS[N_ROWS] = { "> DATE ......", "> LINK ......", "> PROX ......", "> HOST ......", "> PWR  ......" };
static const char  PROMPT[] = "root@tft:~$ ./chrono --live";

static lv_obj_t *prompt, *big, *secs, *ampm, *key[N_ROWS], *val[N_ROWS], *sync, *cursor;
static uint32_t  t0;
static int       shown_prompt, c_cursor, c_prox_red, keys_shown, c_cur_x;
static char      cache[10][32];

static void create(lv_obj_t *scr)
{
    prompt = clock_label(scr, &lv_font_montserrat_14, C_DIM, 0);
    lv_obj_set_pos(prompt, 10, 32);

    big = clock_label(scr, &lv_font_digits_64, C_GRN, 0);
    lv_obj_set_pos(big, 10, 46);
    secs = clock_label(scr, &lv_font_montserrat_28_deg, C_GRN, 0);
    lv_obj_set_pos(secs, 190, 74);
    ampm = clock_label(scr, &lv_font_montserrat_14, C_DIM, 1);
    lv_obj_set_pos(ampm, 194, 58);
    clock_tap_area(scr, 6, 46, 240, 64);

    for (int i = 0; i < N_ROWS; i++)
    {
        key[i] = clock_label(scr, &lv_font_montserrat_14, C_GRN, 0);
        lv_obj_set_pos(key[i], 10, 120 + i * 17);
        val[i] = clock_label(scr, &lv_font_montserrat_14, C_GRN, 0);
        lv_obj_set_pos(val[i], 122, 120 + i * 17);
    }
    sync = clock_label(scr, &lv_font_montserrat_14, C_DIM, 0);
    lv_obj_set_pos(sync, 10, 210);
    cursor = clock_box(scr, 0, 212, 8, 14, C_GRN);
    lv_obj_set_style_radius(cursor, 0, 0);

    memset(cache, 0, sizeof cache);
    t0 = HAL_GetTick();
    shown_prompt = -1; c_cursor = -1; c_prox_red = -1; keys_shown = 0; c_cur_x = -1;
}

static void update(const clock_data_t *d)
{
    static const char *const WD[8] = { "", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN" };
    char b[32];
    uint32_t el = d->now - t0;                         /* ms since the screen opened */

    /* intro: the prompt types itself (25 ms per char), then each row appears */
    int n = (int)(el / 25U);
    if (n > (int)sizeof PROMPT - 1) n = (int)sizeof PROMPT - 1;
    if (n != shown_prompt)
    {
        shown_prompt = n;
        memcpy(b, PROMPT, (size_t)n);
        b[n] = '\0';
        lv_label_set_text(prompt, b);
    }
    uint32_t base = (sizeof PROMPT - 1) * 25U;          /* prompt done */
    int rows = el < base ? -1 : (int)((el - base) / 150U);  /* -1 = time not shown yet */

    if (rows >= 0)
    {
        if (d->have)
        {
            lv_snprintf(b, sizeof b, "%02u:%02u", d->hour, d->t.minute);  clock_text(big, cache[0], 32, b);
            lv_snprintf(b, sizeof b, ":%02u", d->t.second);               clock_text(secs, cache[1], 32, b);
            clock_text(ampm, cache[2], 32, d->h24 ? "24H" : d->ampm);
        }
        else
        {
            clock_text(big, cache[0], 32, "--:--");
            clock_text(secs, cache[1], 32, "");
            clock_text(ampm, cache[2], 32, "");
        }
    }

    for (int i = 0; i < N_ROWS; i++)
    {
        if (i >= rows) continue;                       /* not revealed yet */
        if (i >= keys_shown) { lv_label_set_text_static(key[i], KEYS[i]); keys_shown = i + 1; }
        switch (i)
        {
            case 0:
                if (d->have) lv_snprintf(b, sizeof b, "%u-%02u-%02u %s", d->t.year, d->t.month, d->t.day, WD[d->t.weekday]);
                else         lv_snprintf(b, sizeof b, "----------");
                break;
            case 1:
            {
                int lv = !d->in.phone ? 0 : d->dbm > -55 ? 5 : d->dbm > -62 ? 4 : d->dbm > -70 ? 3 : d->dbm > -80 ? 2 : 1;
                if (!d->in.phone) lv_snprintf(b, sizeof b, "NO LINK");
                else lv_snprintf(b, sizeof b, "iPHONE [%.*s%.*s]", lv, "#####", 5 - lv, "-----");
                break;
            }
            case 2:
                if (!d->in.phone)      lv_snprintf(b, sizeof b, "---");
                else if (d->in.paused) lv_snprintf(b, sizeof b, "PAUSED");
                else lv_snprintf(b, sizeof b, "%s  %ddBm", d->in.near ? "NEAR" : "FAR", d->dbm);
                {
                    int red = d->in.phone && !d->in.paused && !d->in.near;
                    if (red != c_prox_red) { c_prox_red = red; lv_obj_set_style_text_color(val[2], lv_color_hex(red ? C_RED : C_GRN), 0); }
                }
                break;
            case 3:
                lv_snprintf(b, sizeof b, "%s", d->in.kbd ? "ARMED" : (d->in.laptops ? "LINKED" : "---"));
                break;
            default:
                if (d->bat >= 0) lv_snprintf(b, sizeof b, "%d%%", d->bat); else lv_snprintf(b, sizeof b, "--");
                break;
        }
        clock_text(val[i], cache[3 + i], 32, b);
    }

    if (rows >= N_ROWS)
    {
        if (d->sync_ago == 0xFFFFFFFFU) lv_snprintf(b, sizeof b, "> waiting for iphone time");
        else if (d->sync_ago < 120)     lv_snprintf(b, sizeof b, "> sync ok");
        else lv_snprintf(b, sizeof b, "> sync %lum ago", (unsigned long)(d->sync_ago / 60U));
        clock_text(sync, cache[8], 32, b);
        lv_obj_update_layout(sync);
        int x = 10 + lv_obj_get_width(sync) + 4;        /* cursor right after the text */
        if (x != c_cur_x) { c_cur_x = x; lv_obj_set_x(cursor, x); }
    }
    int on = (d->now / 500U) & 1;                      /* blinking block cursor */
    if (on != c_cursor) { c_cursor = on; lv_obj_set_style_bg_opa(cursor, on ? LV_OPA_COVER : LV_OPA_TRANSP, 0); }
}

const clock_layout_t CLOCK_TERM = { "TERM", C_BG, C_GRN, 100, create, update };
