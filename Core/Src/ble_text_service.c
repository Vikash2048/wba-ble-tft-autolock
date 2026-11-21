/* Step 5.3 – "Message" service: text written by the phone is shown on the TFT.
 *
 * How a write from the phone reaches us:
 *   phone writes the characteristic -> radio -> BLE stack
 *   -> stack raises event ACI_GATT_ATTRIBUTE_MODIFIED (because we asked for it with
 *      GATT_NOTIFY_ATTRIBUTE_WRITE)
 *   -> ST's service controller (svc_ctl) calls every registered handler
 *   -> Text_EventHandler() below checks the attribute handle and copies the text.
 * The handler runs in ST's task scheduler (main loop), the same thread as LVGL,
 * so the screen can simply read the buffer. */
#include "main.h"                     /* same include order as ST's app_ble.c */
#include "app_common.h"
#include "ble_core.h"
#include "svc_ctl.h"
#include "ble_text_service.h"

/* 128-bit UUIDs, least significant byte first (that is how the BLE stack wants them) */
static const uint8_t TEXT_SERVICE_UUID[16] =     /* A07E0001-5A1E-4C0D-9E2B-7F3C1D0E2A10 */
    { 0x10, 0x2A, 0x0E, 0x1D, 0x3C, 0x7F, 0x2B, 0x9E, 0x0D, 0x4C, 0x1E, 0x5A, 0x01, 0x00, 0x7E, 0xA0 };
static const uint8_t TEXT_CHAR_UUID[16] =        /* A07E0002-5A1E-4C0D-9E2B-7F3C1D0E2A10 */
    { 0x10, 0x2A, 0x0E, 0x1D, 0x3C, 0x7F, 0x2B, 0x9E, 0x0D, 0x4C, 0x1E, 0x5A, 0x02, 0x00, 0x7E, 0xA0 };

#define CHAR_VALUE_OFFSET  1        /* value attribute = characteristic handle + 1 */

static uint16_t svc_handle;
static uint16_t text_char_handle;
static char     text[BLE_TEXT_MAX + 1];
static uint32_t msg_count;
static uint32_t msg_time;

static SVCCTL_EvtAckStatus_t Text_EventHandler(void *p_Event)
{
    hci_event_pckt *p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);

    if (p_event_pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
        return SVCCTL_EvtNotAck;

    evt_blecore_aci *p_blecore_evt = (evt_blecore_aci *)p_event_pckt->data;
    if (p_blecore_evt->ecode != ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE)
        return SVCCTL_EvtNotAck;

    aci_gatt_attribute_modified_event_rp0 *p_attr =
        (aci_gatt_attribute_modified_event_rp0 *)p_blecore_evt->data;
    if (p_attr->Attr_Handle != text_char_handle + CHAR_VALUE_OFFSET)
        return SVCCTL_EvtNotAck;                    /* not ours: let other handlers look */

    uint16_t n = p_attr->Attr_Data_Length;
    if (n > BLE_TEXT_MAX) n = BLE_TEXT_MAX;
    for (uint16_t i = 0; i < n; i++) {
        char c = (char)p_attr->Attr_Data[i];
        text[i] = (c == '\r') ? ' ' : c;            /* keep '\n' as a line break */
    }
    text[n] = '\0';
    msg_count++;
    msg_time = HAL_GetTick();
    return SVCCTL_EvtAckFlowEnable;                 /* handled */
}

void ble_text_service_init(void)
{
    Service_UUID_t svc_uuid;
    Char_UUID_t    char_uuid;

    for (int i = 0; i < 16; i++) svc_uuid.Service_UUID_128[i] = TEXT_SERVICE_UUID[i];
    if (aci_gatt_add_service(UUID_TYPE_128, &svc_uuid, PRIMARY_SERVICE,
                             3,                       /* service + char declaration + value */
                             &svc_handle) != BLE_STATUS_SUCCESS)
        return;

    for (int i = 0; i < 16; i++) char_uuid.Char_UUID_128[i] = TEXT_CHAR_UUID[i];
    if (aci_gatt_add_char(svc_handle, UUID_TYPE_128, &char_uuid,
                          BLE_TEXT_MAX,               /* maximum length */
                          CHAR_PROP_READ | CHAR_PROP_WRITE | CHAR_PROP_WRITE_WITHOUT_RESP,
                          ATTR_PERMISSION_NONE,
                          GATT_NOTIFY_ATTRIBUTE_WRITE, /* -> ACI_GATT_ATTRIBUTE_MODIFIED event */
                          0x10,
                          CHAR_VALUE_LEN_VARIABLE,    /* 1 .. BLE_TEXT_MAX bytes */
                          &text_char_handle) != BLE_STATUS_SUCCESS)
        return;

    SVCCTL_RegisterSvcHandler(Text_EventHandler);
}

const char *ble_text_last(void)    { return text; }
uint32_t    ble_text_count(void)   { return msg_count; }
uint32_t    ble_text_time_ms(void) { return msg_time; }
