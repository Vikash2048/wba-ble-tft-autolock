#ifndef AMS_CLIENT_H
#define AMS_CLIENT_H
#include <stdint.h>

/* Step 7 – "Now playing" from an iPhone: Apple Media Service (AMS) client.
 *
 * iOS has AMS built in (no app needed). The iPhone is the GATT *server* for AMS and
 * our board is the *client*: after the iPhone connects, we look for the AMS service
 * on the phone, subscribe to "Entity Update" and tell it which values we want
 * (player name, play state, title, artist, duration). From then on iOS sends a
 * notification every time the song or the play state changes – for any app that
 * shows up in the iOS "Now Playing" controls (Music, Spotify, YouTube, ...).
 * AMS only works on an encrypted (paired) link, so the board asks to pair once. */

typedef enum {
    AMS_ST_NO_PHONE = 0,   /* nothing connected                                   */
    AMS_ST_SEARCHING,      /* connected, looking for AMS on the phone             */
    AMS_ST_PAIRING,        /* AMS found, waiting for pairing ("Pair" on iPhone)   */
    AMS_ST_READY,          /* subscribed – music info arrives                     */
    AMS_ST_NOT_FOUND,      /* connected device has no AMS (Android, PC, ...)      */
    AMS_ST_ERROR
} ams_state_t;

/* iOS play state (first number of PlaybackInfo) */
#define AMS_PLAY_PAUSED   0
#define AMS_PLAY_PLAYING  1
#define AMS_PLAY_REWIND   2
#define AMS_PLAY_FFWD     3

/* AMS remote commands */
#define AMS_CMD_PLAY        0
#define AMS_CMD_PAUSE       1
#define AMS_CMD_TOGGLE      2
#define AMS_CMD_NEXT        3
#define AMS_CMD_PREVIOUS    4
#define AMS_CMD_VOL_UP      5
#define AMS_CMD_VOL_DOWN    6

typedef struct {
    char     app[32];        /* "Spotify", "YouTube", "Music" ... ("" = nothing playing) */
    char     title[128];
    char     artist[96];
    uint8_t  play_state;     /* AMS_PLAY_* */
    float    duration_s;     /* 0 = unknown */
    uint32_t version;        /* +1 on every change – the screen redraws only then */
} ams_info_t;

void              ams_client_init(void);          /* APP_BLE_Init (app_ble.c)          */
void              ams_client_poll(void);          /* main loop                          */
ams_state_t       ams_client_state(void);
const ams_info_t *ams_client_info(void);
float             ams_client_elapsed_s(void);     /* position now (runs on while playing) */
void              ams_client_command(uint8_t cmd);/* AMS_CMD_*, ignored if not ready     */
void              ams_client_pair(void);          /* ask the phone to pair + search again */
uint32_t          ams_client_pair_code(void);
void              ams_client_diag(char *buf, uint32_t size); /* error details for the screen */     /* 6-digit code shown on the iPhone, 0 = none */

/* hooks called from app_ble.c (USER CODE blocks) */
void ams_on_pairing_complete(uint8_t status);
void ams_on_numeric_comparison(uint32_t value);

#endif
