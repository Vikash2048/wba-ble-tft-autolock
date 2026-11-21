/**
 * presence.h - BLE_AutoLock: is the phone near or far?
 */
#ifndef PRESENCE_H
#define PRESENCE_H

#include <stdint.h>

typedef enum
{
  PRESENCE_FAR = 0,   /* phone away or not connected -> laptop should lock */
  PRESENCE_NEAR
} presence_state_t;

typedef enum
{
  LINK_NONE = 0,      /* no such connection                                */
  LINK_UNKNOWN,       /* connected, not identified yet                     */
  LINK_PHONE,         /* the iPhone (ANCS found)                           */
  LINK_OTHER          /* anything else: the laptop                         */
} presence_link_t;

void presence_init(void);
void presence_on_connect(uint16_t conn_handle);
void presence_on_disconnect(uint16_t conn_handle);
void presence_on_pairing(uint16_t conn_handle, uint8_t status);
void presence_set_scan_response(void);
void presence_toggle_pause(void);   /* Step 5b: B1 */
void presence_forget_all(void);     /* Step 5d (TFT: from the screen, B2 = LCD_RS) */
uint16_t presence_phone_handle(void); /* 0xFFFF = no iPhone identified */
uint8_t  presence_link_count(void);

typedef struct
{
  uint8_t  phone;      /* iPhone connected + identified        */
  uint8_t  laptops;    /* connected links that are not the phone */
  uint8_t  near;       /* 1 = NEAR, 0 = FAR                     */
  uint8_t  paused;
  uint8_t  kbd;        /* a laptop can receive Win+L            */
  int8_t   raw;        /* last RSSI, dBm                        */
  int16_t  filt10;     /* smoothed RSSI x10 (-623 = -62.3 dBm)  */
  uint32_t cond_ms;    /* how long a state change is pending    */
} presence_info_t;
void presence_get_info(presence_info_t *in);
presence_state_t presence_get_state(void);
presence_link_t  presence_link_kind(uint16_t conn_handle);
uint8_t          presence_get_others(uint16_t *out, uint8_t max); /* connected links that are not the phone */

#endif /* PRESENCE_H */
