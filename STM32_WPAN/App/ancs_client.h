/**
 * ancs_client.h - BLE_AutoLock Step 1c
 *
 * Apple Notification Center Service (ANCS) client. iOS has ANCS built in.
 * When a bonded accessory subscribes to ANCS, iOS treats it like a smartwatch:
 * it keeps the link up with no app open and reconnects by itself when the
 * accessory comes back in range. That is exactly the keep-alive we need.
 * Bonus: we get a notification event for every iPhone notification.
 */
#ifndef ANCS_CLIENT_H
#define ANCS_CLIENT_H

#include <stdint.h>

void ancs_client_init(void);                 /* once, at BLE init            */
void ancs_client_on_connect(uint16_t conn);  /* from presence.c              */
void ancs_client_on_disconnect(void);
void ancs_client_on_pairing(uint8_t status);
void ancs_client_tick(void);                 /* every 500 ms (presence task) */
uint8_t ancs_client_ready(void);             /* 1 = subscribed               */
uint8_t ancs_client_failed(void);            /* 1 = no ANCS here (not an iPhone) or error */

#endif /* ANCS_CLIENT_H */
