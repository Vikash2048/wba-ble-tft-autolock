/* Desk clock (menu 13) – shared parts of the 4 layouts.
 *
 *   ui_clock.c        framework: data, timer, [< MENU] [STYLE] buttons, saving the choice
 *   ui_clock_hud.c    A  Cyber HUD
 *   ui_clock_term.c   D  Retro terminal
 *   ui_clock_radar.c  E  Radar
 *   ui_clock_flip.c   F  Flip cards
 *
 * A layout = create() builds its objects on the screen, update() is called by the
 * framework timer with fresh data. Only one layout exists at a time (switching
 * rebuilds the screen), so they don't add up in RAM.
 * Keep the top-left corner free (x < 136, y < 28): the framework buttons go there. */
#ifndef UI_CLOCK_H
#define UI_CLOCK_H

#include "lvgl.h"
#include "phone_time.h"
#include "presence.h"
#include <stddef.h>

LV_FONT_DECLARE(lv_font_montserrat_28_deg);  /* our 28 px font (ui.h) */
LV_FONT_DECLARE(lv_font_digits_64);     /* 64 px digits + ':' (lv_font_digits_64.c) */

typedef struct
{
    int              have;          /* time known (from the iPhone)            */
    phone_datetime_t t;
    uint8_t          h24;           /* 1 = 24 h display                         */
    uint8_t          hour;          /* hour to show: 1..12 or 0..23             */
    const char      *ampm;          /* "AM" / "PM" / "" (24 h)                  */
    uint8_t          minute_changed;/* 1 on the first update of a new minute    */
    presence_info_t  in;            /* phone link, NEAR/FAR, laptop ...         */
    int              dbm;           /* smoothed phone RSSI                      */
    int              bat;           /* phone battery 0..100, -1 unknown         */
    uint32_t         sync_ago;      /* s since time sync, 0xFFFFFFFF = never    */
    uint32_t         now;           /* HAL_GetTick()                            */
} clock_data_t;

typedef struct
{
    const char *name;
    uint32_t    bg, accent;         /* screen background, button colour */
    uint32_t    period_ms;          /* how often update() runs          */
    void      (*create)(lv_obj_t *scr);
    void      (*update)(const clock_data_t *d);
} clock_layout_t;

extern const clock_layout_t CLOCK_HUD, CLOCK_TERM, CLOCK_RADAR, CLOCK_FLIP;

/* helpers for the layouts (ui_clock.c) */
lv_obj_t *clock_box(lv_obj_t *p, int x, int y, int w, int h, uint32_t col);
lv_obj_t *clock_label(lv_obj_t *p, const lv_font_t *f, uint32_t col, int spacing);
void      clock_text(lv_obj_t *l, char *cache, size_t n, const char *s);   /* set only if changed */
lv_obj_t *clock_tap_area(lv_obj_t *scr, int x, int y, int w, int h);       /* tap = 12/24 h */

#endif
