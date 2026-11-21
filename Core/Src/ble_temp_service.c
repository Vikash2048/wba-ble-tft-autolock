/* Step 5.2 – Environmental Sensing service with a Temperature characteristic.
 *
 * GATT table entries created here (4 attributes):
 *   1. Service declaration        0x181A Environmental Sensing
 *   2. Characteristic declaration  properties Read + Notify
 *   3. Characteristic value        0x2A6E Temperature, 2 bytes, sint16, 0.01 C
 *   4. CCCD                        the phone writes it to switch notifications on/off
 *
 * aci_gatt_update_char_value() stores the new value; if the phone has switched
 * notifications on, the BLE stack sends it to the phone automatically. */
#include "main.h"                     /* same include order as ST's app_ble.c */
#include "app_common.h"
#include "ble_core.h"                 /* ACI commands, UUID types, CHAR_PROP_... */
#include "uuid.h"                     /* ENVIRONMENTAL_SENSING_SERVICE_UUID, TEMPERATURE_UUID */
#include "svc_ctl.h"
#include "ble_temp_service.h"
#include "sensors.h"

static uint16_t svc_handle;
static uint16_t temp_char_handle;
static float    last_temp;
static uint32_t update_count;
static uint8_t  ready;

void ble_temp_service_init(void)
{
    Service_UUID_t svc_uuid;
    Char_UUID_t    char_uuid;

    svc_uuid.Service_UUID_16 = ENVIRONMENTAL_SENSING_SERVICE_UUID;    /* 0x181A */
    if (aci_gatt_add_service(UUID_TYPE_16, &svc_uuid, PRIMARY_SERVICE,
                             4,                                       /* attributes, see above */
                             &svc_handle) != BLE_STATUS_SUCCESS)
        return;

    char_uuid.Char_UUID_16 = TEMPERATURE_UUID;                        /* 0x2A6E */
    if (aci_gatt_add_char(svc_handle, UUID_TYPE_16, &char_uuid,
                          2,                                          /* value length: sint16 */
                          CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                          ATTR_PERMISSION_NONE,                       /* no pairing needed */
                          GATT_DONT_NOTIFY_EVENTS,                    /* app needs no events */
                          0x10,                                       /* encryption key size */
                          CHAR_VALUE_LEN_CONSTANT,
                          &temp_char_handle) != BLE_STATUS_SUCCESS)
        return;

    ready = 1;
}

void ble_temp_service_update(float temp_c)
{
    if (!ready) return;

    /* sint16, 0.01 C, little endian (Bluetooth byte order) */
    int16_t v = (int16_t)(temp_c * 100.0f + (temp_c >= 0 ? 0.5f : -0.5f));
    uint8_t buf[2] = { (uint8_t)(v & 0xFF), (uint8_t)((uint16_t)v >> 8) };

    if (aci_gatt_update_char_value(svc_handle, temp_char_handle, 0, 2, buf) == BLE_STATUS_SUCCESS) {
        last_temp = temp_c;
        update_count++;
    }
}

float    ble_temp_service_last(void)    { return last_temp; }
uint32_t ble_temp_service_updates(void) { return update_count; }

/* Hook from sensors.c: called after every new temperature sample (once per second).
 * sensors.c has an empty default (weak) version, so tft_screen_impl needs no BLE. */
void sensors_on_new_sample(float temp_c)
{
    ble_temp_service_update(temp_c);
}
