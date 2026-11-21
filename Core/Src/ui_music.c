/* Step 7 – "Now playing" screen: song from the iPhone (Apple Media Service, ams_client.c).
 *
 *   < Menu                         Now playing
 *   SPOTIFY  > Playing
 *   Song title (scrolls if long)
 *   Artist
 *   [=========-----------]
 *   1:23                       3:45
 *        [ |<< ]  [ > / || ]  [ >>| ]
 *   status line (connection / pairing)
 *
 * B1: tap = next button, hold = press it (same as every screen). */
#include "main.h"
#include "ui.h"
#include "ams_client.h"

static lv_obj_t   *app_label, *title_label, *artist_label, *bar, *pos_label, *dur_label;
static lv_obj_t   *play_label, *status_label;
static lv_timer_t *refresh_timer;
static uint32_t    shown_version = 0xFFFFFFFF;
static int         shown_state = -1, shown_pos = -1;

static void fmt_time(char *b, uint32_t size, float s)
{
    uint32_t t = (uint32_t)s;
    lv_snprintf(b, size, "%lu:%02lu", (unsigned long)(t / 60), (unsigned long)(t % 60));
}

static void update_status(ams_state_t st)
{
    char b[80];
    switch (st) {
    case AMS_ST_NO_PHONE:
        lv_label_set_text(status_label, "Not connected. iPhone: Settings > Bluetooth > TFT_xx");
        break;
    case AMS_ST_SEARCHING:
        lv_label_set_text(status_label, "iPhone connected - looking for its media service...");
        break;
    case AMS_ST_PAIRING:
        if (ams_client_pair_code()) {
            lv_snprintf(b, sizeof b, "Pairing code %06lu - tap Pair on the iPhone",
                     (unsigned long)ams_client_pair_code());
            lv_label_set_text(status_label, b);
        } else {
            lv_label_set_text(status_label, "Tap \"Pair\" on the iPhone");
        }
        break;
    case AMS_ST_READY:
        lv_label_set_text(status_label, ams_client_info()->app[0]
                          ? "Connected to iPhone" : "Ready - start music on the iPhone");
        break;
    case AMS_ST_NOT_FOUND:
        lv_label_set_text(status_label, "No Apple media service found - press Pair (iPhone only)");
        break;
    default: {
        char d[40];
        ams_client_diag(d, sizeof d);
        lv_snprintf(b, sizeof b, "Error %s - Pair = retry", d);
        lv_label_set_text(status_label, b);
        break;
    }
    }
}

static void refresh(void)
{
    const ams_info_t *in = ams_client_info();
    ams_state_t st = ams_client_state();
    char b[48];

    if (in->version != shown_version || (int)st != shown_state) {
        shown_version = in->version;
        shown_state = st;

        if (st == AMS_ST_READY && in->app[0]) {
            const char *ps = in->play_state == AMS_PLAY_PLAYING ? LV_SYMBOL_PLAY "  Playing" :
                             in->play_state == AMS_PLAY_PAUSED  ? LV_SYMBOL_PAUSE "  Paused" :
                                                                  LV_SYMBOL_SHUFFLE "  Seeking";
            lv_snprintf(b, sizeof b, "%s   %s", in->app, ps);
            lv_label_set_text(app_label, b);
            lv_label_set_text(title_label, in->title[0] ? in->title : "(no title)");
            lv_label_set_text(artist_label, in->artist);
        } else {
            lv_label_set_text(app_label, "");
            lv_label_set_text(title_label, st == AMS_ST_READY ? "Nothing playing" : "No music info");
            lv_label_set_text(artist_label, "");
        }
        if (st == AMS_ST_READY)
            lv_label_set_text(play_label, in->play_state == AMS_PLAY_PLAYING ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        else
            lv_label_set_text(play_label, "Pair");

        if (in->duration_s > 0) fmt_time(b, sizeof b, in->duration_s); else b[0] = '\0';
        lv_label_set_text(dur_label, b);
        update_status(st);
        shown_pos = -1;                               /* force position update */
    }

    /* position: only redraw when the shown second changes */
    float pos = ams_client_elapsed_s();
    if (st != AMS_ST_READY || !in->app[0]) pos = 0;
    if ((int)pos != shown_pos) {
        shown_pos = (int)pos;
        fmt_time(b, sizeof b, pos);
        lv_label_set_text(pos_label, (st == AMS_ST_READY && in->app[0]) ? b : "");
        int32_t pct = in->duration_s > 0 ? (int32_t)(pos * 1000.0f / in->duration_s) : 0;
        lv_bar_set_value(bar, pct, LV_ANIM_OFF);
    }
}

static void refresh_timer_cb(lv_timer_t *t) { (void)t; refresh(); }

static void screen_deleted_cb(lv_event_t *e)
{
    (void)e;
    lv_timer_delete(refresh_timer);
    refresh_timer = NULL;
}

static void btn_cb(lv_event_t *e)
{
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which == 0) ams_client_command(AMS_CMD_PREVIOUS);
    else if (which == 2) ams_client_command(AMS_CMD_NEXT);
    else if (ams_client_state() == AMS_ST_READY) ams_client_command(AMS_CMD_TOGGLE);
    else ams_client_pair();
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, "");
    return l;
}

void ui_show_music(void)
{
    lv_obj_t *scr = ui_new_screen(NULL);
    lv_obj_add_event_cb(scr, screen_deleted_cb, LV_EVENT_DELETE, NULL);
    lv_obj_t *back = ui_add_back_button(scr);

    lv_obj_t *head = label(scr, &lv_font_montserrat_20, 0xFFFFFF);
    lv_label_set_text(head, LV_SYMBOL_AUDIO "  Now playing");
    lv_obj_align(head, LV_ALIGN_TOP_RIGHT, -10, 14);

    app_label = label(scr, &lv_font_montserrat_14, 0x00D0FF);
    lv_obj_set_pos(app_label, 12, 50);

    title_label = label(scr, &lv_font_montserrat_28_deg, 0xFFFFFF);
    lv_obj_set_width(title_label, 296);
    lv_label_set_long_mode(title_label, LV_LABEL_LONG_SCROLL_CIRCULAR);   /* marquee for long titles */
    lv_obj_set_pos(title_label, 12, 68);

    artist_label = label(scr, &lv_font_montserrat_20, 0xB0B0B0);
    lv_obj_set_width(artist_label, 296);
    lv_label_set_long_mode(artist_label, LV_LABEL_LONG_DOT);
    lv_obj_set_pos(artist_label, 12, 102);

    bar = lv_bar_create(scr);
    lv_obj_set_size(bar, 296, 8);
    lv_obj_set_pos(bar, 12, 132);
    lv_bar_set_range(bar, 0, 1000);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x303840), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x00D0FF), LV_PART_INDICATOR);

    pos_label = label(scr, &lv_font_montserrat_14, 0x909090);
    lv_obj_set_pos(pos_label, 12, 144);
    dur_label = label(scr, &lv_font_montserrat_14, 0x909090);
    lv_obj_align(dur_label, LV_ALIGN_TOP_RIGHT, -12, 144);

    /* controls: previous, play/pause (or "Pair"), next */
    static const char *const sym[3] = { LV_SYMBOL_PREV, LV_SYMBOL_PLAY, LV_SYMBOL_NEXT };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_button_create(scr);
        lv_obj_set_size(b, i == 1 ? 90 : 70, 44);
        lv_obj_align(b, LV_ALIGN_TOP_MID, (i - 1) * 95, 164);
        lv_obj_add_event_cb(b, btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = label(b, &lv_font_montserrat_20, 0xFFFFFF);
        lv_label_set_text(l, sym[i]);
        lv_obj_center(l);
        if (i == 1) play_label = l;
    }

    status_label = label(scr, &lv_font_montserrat_14, 0x909090);
    lv_obj_set_width(status_label, 300);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_DOT);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_LEFT, 10, -6);

    shown_version = 0xFFFFFFFF; shown_state = -1; shown_pos = -1;
    refresh();
    refresh_timer = lv_timer_create(refresh_timer_cb, 200, NULL);
    ui_load(scr, back);
}
