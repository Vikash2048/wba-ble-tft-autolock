/* Step 6 – "Image" service: a picture from the phone is streamed onto the TFT.
 *
 * Flow:
 *   1. Phone writes START (width, height) to Control.
 *      -> we centre the picture, set the display window once (tft_stream_begin)
 *         and set busy = 1, so main.c stops calling lv_timer_handler().
 *         (LVGL must not draw now: it would move the display window.)
 *   2. Phone writes Data chunks (Write Without Response = no reply per packet, fast).
 *      -> each chunk goes straight to the display (tft_stream_bytes).
 *   3. When width*height*2 bytes have arrived: state DONE, busy = 0, LVGL runs again.
 *      LVGL only redraws areas that change, so the picture stays on the screen.
 *   On ABORT / 2 s without data / disconnect: state ERROR and the screen is redrawn.
 *
 * Status is notified to the phone about every 10 % and at the end.
 * All of this runs in ST's task scheduler (main loop) – same thread as LVGL. */
#include "main.h"                     /* same include order as ST's app_ble.c */
#include "app_common.h"
#include "ble_core.h"
#include "svc_ctl.h"
#include "tft.h"
#include "lvgl.h"
#include "ble_ui.h"
#include "ble_image_service.h"

/* 128-bit UUIDs, least significant byte first. Only byte [12] differs. */
#define IMG_UUID(b) { 0x10, 0x2A, 0x0E, 0x1D, 0x3C, 0x7F, 0x2B, 0x9E, \
                      0x0D, 0x4C, 0x1E, 0x5A, (b), 0x00, 0x7E, 0xA0 }
static const uint8_t IMG_SERVICE_UUID[16] = IMG_UUID(0x10);   /* A07E0010-... */
static const uint8_t IMG_CTRL_UUID[16]    = IMG_UUID(0x11);   /* A07E0011-... */
static const uint8_t IMG_DATA_UUID[16]    = IMG_UUID(0x12);   /* A07E0012-... */
static const uint8_t IMG_STATUS_UUID[16]  = IMG_UUID(0x13);   /* A07E0013-... */

#define CHAR_VALUE_OFFSET  1          /* value attribute = characteristic handle + 1 */
#define CMD_START          0x01
#define CMD_ABORT          0x02
#define TIMEOUT_MS         2000

static uint16_t svc_handle, ctrl_handle, data_handle, status_handle;
static volatile int screen_open;

static uint8_t  state = BLE_IMG_ST_IDLE, error;
static int      busy;
static uint16_t img_w, img_h;
static uint32_t expected, received;
static uint32_t last_rx_ms, start_ms, last_ms, count;
static uint8_t  last_pct_sent;

static void send_status(void)
{
    uint8_t pct = expected ? (uint8_t)((uint64_t)received * 100U / expected) : 0;
    uint8_t s[8] = { state, error,
                     (uint8_t)received, (uint8_t)(received >> 8),
                     (uint8_t)(received >> 16), (uint8_t)(received >> 24),
                     pct, 0 };
    /* stores the value; sent as a notification if the phone has subscribed */
    aci_gatt_update_char_value(svc_handle, status_handle, 0, sizeof s, s);
    last_pct_sent = pct;
}

static void finish(uint8_t new_state, uint8_t err)
{
    state = new_state;
    error = err;
    if (busy) {
        busy = 0;
        if (new_state == BLE_IMG_ST_ERROR)
            lv_obj_invalidate(lv_screen_active());   /* half picture: let LVGL repaint */
    }
    send_status();
}

static void on_control(const uint8_t *d, uint16_t n)
{
    if (n >= 1 && d[0] == CMD_ABORT) {
        if (busy) finish(BLE_IMG_ST_ERROR, BLE_IMG_ERR_ABORTED);
        return;
    }
    if (n < 5 || d[0] != CMD_START) return;

    if (busy) finish(BLE_IMG_ST_ERROR, BLE_IMG_ERR_ABORTED);   /* new START cancels old */

    uint16_t w = (uint16_t)(d[1] | (d[2] << 8));
    uint16_t h = (uint16_t)(d[3] | (d[4] << 8));
    received = 0; expected = 0;
    if (!screen_open)                                { finish(BLE_IMG_ST_ERROR, BLE_IMG_ERR_NOT_OPEN); return; }
    if (w == 0 || h == 0 || w > TFT_WIDTH || h > TFT_HEIGHT)
                                                     { finish(BLE_IMG_ST_ERROR, BLE_IMG_ERR_BAD_SIZE); return; }

    img_w = w; img_h = h;
    expected = (uint32_t)w * h * 2U;
    uint16_t x0 = (TFT_WIDTH - w) / 2, y0 = (TFT_HEIGHT - h) / 2;   /* centred */
    tft_stream_begin(x0, y0, x0 + w - 1, y0 + h - 1);

    busy = 1;
    state = BLE_IMG_ST_RECEIVING;
    error = 0;
    start_ms = last_rx_ms = HAL_GetTick();
    send_status();
}

static void on_data(const uint8_t *d, uint16_t n)
{
    if (!busy) return;                                /* data without START: ignore */
    if (received + n > expected) n = (uint16_t)(expected - received);
    tft_stream_bytes(d, n);
    received += n;
    last_rx_ms = HAL_GetTick();

    if (received >= expected) {
        last_ms = HAL_GetTick() - start_ms;
        count++;
        finish(BLE_IMG_ST_DONE, 0);
    } else if ((uint8_t)(received * 100U / expected) >= last_pct_sent + 10) {
        send_status();
    }
}

static SVCCTL_EvtAckStatus_t Image_EventHandler(void *p_Event)
{
    hci_event_pckt *p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);
    if (p_event_pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
        return SVCCTL_EvtNotAck;

    evt_blecore_aci *p_blecore_evt = (evt_blecore_aci *)p_event_pckt->data;
    if (p_blecore_evt->ecode != ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE)
        return SVCCTL_EvtNotAck;

    aci_gatt_attribute_modified_event_rp0 *p_attr =
        (aci_gatt_attribute_modified_event_rp0 *)p_blecore_evt->data;

    if (p_attr->Attr_Handle == data_handle + CHAR_VALUE_OFFSET) {
        on_data(p_attr->Attr_Data, p_attr->Attr_Data_Length);
        return SVCCTL_EvtAckFlowEnable;
    }
    if (p_attr->Attr_Handle == ctrl_handle + CHAR_VALUE_OFFSET) {
        on_control(p_attr->Attr_Data, p_attr->Attr_Data_Length);
        return SVCCTL_EvtAckFlowEnable;
    }
    return SVCCTL_EvtNotAck;                          /* not ours (also the CCCD write) */
}

static int add_char(const uint8_t *uuid, uint16_t len, uint8_t props, uint8_t events,
                    uint8_t var_len, uint16_t *handle)
{
    Char_UUID_t u;
    for (int i = 0; i < 16; i++) u.Char_UUID_128[i] = uuid[i];
    return aci_gatt_add_char(svc_handle, UUID_TYPE_128, &u, len, props,
                             ATTR_PERMISSION_NONE, events, 0x10, var_len, handle)
           == BLE_STATUS_SUCCESS;
}

void ble_image_service_init(void)
{
    Service_UUID_t svc_uuid;
    for (int i = 0; i < 16; i++) svc_uuid.Service_UUID_128[i] = IMG_SERVICE_UUID[i];

    /* attributes: service 1 + control 2 + data 2 + status 3 (decl, value, CCCD) = 8 */
    if (aci_gatt_add_service(UUID_TYPE_128, &svc_uuid, PRIMARY_SERVICE, 8,
                             &svc_handle) != BLE_STATUS_SUCCESS)
        return;

    if (!add_char(IMG_CTRL_UUID, 5, CHAR_PROP_WRITE,
                  GATT_NOTIFY_ATTRIBUTE_WRITE, CHAR_VALUE_LEN_VARIABLE, &ctrl_handle)) return;
    if (!add_char(IMG_DATA_UUID, BLE_IMAGE_CHUNK, CHAR_PROP_WRITE_WITHOUT_RESP,
                  GATT_NOTIFY_ATTRIBUTE_WRITE, CHAR_VALUE_LEN_VARIABLE, &data_handle)) return;
    if (!add_char(IMG_STATUS_UUID, 8, CHAR_PROP_READ | CHAR_PROP_NOTIFY,
                  GATT_DONT_NOTIFY_EVENTS, CHAR_VALUE_LEN_CONSTANT, &status_handle)) return;

    SVCCTL_RegisterSvcHandler(Image_EventHandler);
}

int ble_image_busy(void)
{
    if (busy) {
        if (ble_ui_get_state() != BLE_UI_CONNECTED)
            finish(BLE_IMG_ST_ERROR, BLE_IMG_ERR_TIMEOUT);      /* phone went away */
        else if (HAL_GetTick() - last_rx_ms > TIMEOUT_MS)
            finish(BLE_IMG_ST_ERROR, BLE_IMG_ERR_TIMEOUT);
    }
    return busy;
}

void     ble_image_set_screen_open(int open) { screen_open = open; }
uint8_t  ble_image_state(void)   { return state; }
uint8_t  ble_image_error(void)   { return error; }
uint32_t ble_image_count(void)   { return count; }
uint32_t ble_image_last_ms(void) { return last_ms; }
uint16_t ble_image_width(void)   { return img_w; }
uint16_t ble_image_height(void)  { return img_h; }
