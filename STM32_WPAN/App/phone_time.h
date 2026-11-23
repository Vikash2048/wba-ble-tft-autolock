/**
 * phone_time.h - Smart desk clock: time + battery from the iPhone over BLE.
 *
 * iOS offers two standard Bluetooth services to paired accessories (no app needed):
 *   Current Time Service 0x1805 -> Current Time 0x2A2B (read + notify)
 *   Battery Service      0x180F -> Battery Level 0x2A19 (read + notify)
 * The board is a GATT *client* on the phone link (like ANCS / AMS).
 */
#ifndef PHONE_TIME_H
#define PHONE_TIME_H

#include <stdint.h>

typedef struct
{
  uint16_t year;
  uint8_t  month, day;           /* 1..12, 1..31          */
  uint8_t  hour, minute, second; /* 24 h                  */
  uint8_t  weekday;              /* 1 = Monday .. 7 = Sunday */
} phone_datetime_t;

void     phone_time_init(void);                 /* APP_BLE_Init: register event handler       */
void     phone_time_tick(void);                 /* every 500 ms (presence.c task)             */
int      phone_time_now(phone_datetime_t *t);   /* 1 = time known (running on since last sync) */
int      phone_battery(void);                   /* 0..100, -1 = unknown                       */
uint32_t phone_time_synced_ago_s(void);         /* seconds since the last time from the phone */

#endif /* PHONE_TIME_H */
