/* Step 7 – Apple Media Service (AMS) client: "Now playing" from an iPhone.
 *
 * Sequence after the iPhone has connected (one GATT procedure at a time; every
 * procedure ends with the event ACI_GATT_PROC_COMPLETE, which moves us on):
 *
 *   1. find the AMS service on the phone      aci_gatt_disc_primary_service_by_uuid
 *   2. find its 3 characteristics             aci_gatt_disc_all_char_of_service
 *   3. switch on notifications of
 *      "Entity Update" (write 01 00 to CCCD)   aci_gatt_write_char_value
 *        -> phone answers "insufficient authentication" if not paired yet:
 *           we send a security request, iPhone shows "Pair", then we retry
 *   4. register player attributes             write 00 00 01   (Player: Name, PlaybackInfo)
 *   5. register track attributes              write 02 00 02 03 (Track: Artist, Title, Duration)
 *   6. READY: notifications arrive            [EntityID][AttributeID][Flags][UTF-8 value]
 *
 * Everything runs in the main loop (ST's sequencer + ams_client_poll), no RTOS. */
#include "main.h"                     /* same include order as ST's app_ble.c */
#include "app_common.h"
#include "ble_core.h"
#include "svc_ctl.h"
#include "ble_ui.h"
#include "ams_client.h"
#include <string.h>

/* 128-bit UUIDs, least significant byte first */
static const uint8_t UUID_AMS[16] = { 0xDC, 0xF8, 0x55, 0xAD, 0x02, 0xC5, 0xF4, 0x8E,
                                      0x3A, 0x43, 0x36, 0x0F, 0x2B, 0x50, 0xD3, 0x89 }; /* 89D3502B-0F36-433A-8EF4-C502AD55F8DC */
static const uint8_t UUID_RC[16]  = { 0xC2, 0x51, 0xCA, 0xF7, 0x56, 0x0E, 0xDF, 0xB8,
                                      0x8A, 0x4A, 0xB1, 0x57, 0xD8, 0x81, 0x3C, 0x9B }; /* Remote Command */
static const uint8_t UUID_EU[16]  = { 0x02, 0xC1, 0x96, 0xBA, 0x92, 0xBB, 0x0C, 0x9A,
                                      0x1F, 0x41, 0x8D, 0x80, 0xCE, 0xAB, 0x7C, 0x2F }; /* Entity Update  */

/* AMS entity / attribute IDs */
#define ENT_PLAYER   0
#define ENT_TRACK    2
#define PL_NAME      0
#define PL_PLAYBACK  1
#define TR_ARTIST    0
#define TR_TITLE     2
#define TR_DURATION  3

/* ATT error codes that mean "pair first" */
#define ATT_ERR_INSUF_AUTHENTICATION  0x05
#define ATT_ERR_INSUF_ENCRYPTION      0x0F

typedef enum {                        /* internal steps */
    S_IDLE, S_WAIT, S_DISC_SVC, S_DISC_CHAR, S_DISC_DESC, S_SUBSCRIBE, S_WAIT_PAIR,
    S_REG_PLAYER, S_REG_TRACK, S_READY, S_NOT_FOUND, S_ERROR
} step_t;

static step_t   step = S_IDLE;
static int      proc_busy;            /* a GATT procedure of ours is running   */
static uint8_t  att_error;            /* ATT error of the running procedure    */
static uint16_t conn = 0xFFFF;
static uint16_t svc_start, svc_end;
static uint16_t rc_handle, eu_handle; /* value handles on the phone            */
static uint16_t eu_cccd;              /* "notifications on/off" switch of EU   */
/* diagnostics shown on the screen when something fails */
static uint8_t  d_step, d_api, d_proc, d_att;
static uint32_t next_try_ms, wait_since_ms;
static uint8_t  tries;
static int      cmd_pending = -1;
static uint32_t pair_code;
static uint32_t sec_req_ms;           /* last security request (don't spam the phone) */

static ams_info_t info;
static float    elapsed_s, rate;      /* from PlaybackInfo                     */
static uint32_t elapsed_tick;         /* HAL tick when elapsed_s was received  */

/* ---------------- helpers ---------------- */
static void clear_info(void)
{
    info.app[0] = info.title[0] = info.artist[0] = '\0';
    info.play_state = AMS_PLAY_PAUSED;
    info.duration_s = 0;
    elapsed_s = 0; rate = 0;
    info.version++;
}

static float parse_float(const char **pp)       /* "123.456" -> 123.456 (small, no libc) */
{
    const char *p = *pp;
    float v = 0, scale = 1;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { scale /= 10; v += (*p++ - '0') * scale; } }
    *pp = p;
    return neg ? -v : v;
}

static void copy_str(char *dst, uint32_t size, const uint8_t *src, uint32_t n, int truncated)
{
    if (n >= size) { n = size - 1; truncated = 1; }
    memcpy(dst, src, n);
    dst[n] = '\0';
    if (truncated && n >= 3) { dst[n - 3] = dst[n - 2] = dst[n - 1] = '.'; }
}

static int start_proc(tBleStatus st)              /* 1 = started */
{
    if (st == BLE_STATUS_SUCCESS) { proc_busy = 1; att_error = 0; next_try_ms = 0; return 1; }
    /* stack busy (MTU exchange, pairing popup still open on the phone ...): retry for 60 s */
    next_try_ms = HAL_GetTick() + 500;
    d_api = (uint8_t)st;
    if (++tries > 120) { d_step = (uint8_t)step; step = S_ERROR; }
    return 0;
}

static int write_value(uint16_t handle, const uint8_t *v, uint8_t len)
{
    return start_proc(aci_gatt_write_char_value(conn, handle, len, v));
}

/* run the action that belongs to the current step */
static void do_step(void)
{
    static const uint8_t cccd_on[2] = { 0x01, 0x00 };
    static const uint8_t reg_player[3] = { ENT_PLAYER, PL_NAME, PL_PLAYBACK };
    static const uint8_t reg_track[4]  = { ENT_TRACK, TR_ARTIST, TR_TITLE, TR_DURATION };

    switch (step) {
    case S_DISC_SVC: {
        UUID_t u;
        memcpy(u.UUID_128, UUID_AMS, 16);
        svc_start = svc_end = 0;
        start_proc(aci_gatt_disc_primary_service_by_uuid(conn, UUID_TYPE_128, &u));
        break;
    }
    case S_DISC_CHAR:
        rc_handle = eu_handle = 0;
        start_proc(aci_gatt_disc_all_char_of_service(conn, svc_start, svc_end));
        break;
    case S_DISC_DESC:                       /* find the CCCD (0x2902) that belongs to Entity Update */
        eu_cccd = 0;
        start_proc(aci_gatt_disc_all_char_desc(conn, svc_start, svc_end));
        break;
    case S_SUBSCRIBE:  write_value(eu_cccd, cccd_on, 2);                 break;
    case S_REG_PLAYER: write_value(eu_handle, reg_player, sizeof reg_player); break;
    case S_REG_TRACK:  write_value(eu_handle, reg_track, sizeof reg_track);   break;
    default: break;
    }
}

static void go(step_t s) { step = s; tries = 0; do_step(); }

/* our procedure finished: decide the next step */
static void fail(uint8_t error_code)
{
    d_step = (uint8_t)step; d_proc = error_code; d_att = att_error;
    step = S_ERROR;
}

static void proc_done(uint8_t error_code)
{
    proc_busy = 0;
    switch (step) {
    case S_DISC_SVC:
        if (svc_start) go(S_DISC_CHAR); else step = S_NOT_FOUND;
        break;
    case S_DISC_CHAR:
        if (eu_handle) go(S_DISC_DESC); else step = S_NOT_FOUND;
        break;
    case S_DISC_DESC:
        if (!eu_cccd) eu_cccd = eu_handle + 1;         /* not reported: usual position */
        go(S_SUBSCRIBE);
        break;
    case S_SUBSCRIBE:
        if (att_error == ATT_ERR_INSUF_AUTHENTICATION || att_error == ATT_ERR_INSUF_ENCRYPTION) {
            if (!sec_req_ms || HAL_GetTick() - sec_req_ms > 15000) {
                aci_gap_peripheral_security_req(conn);   /* iPhone shows "Bluetooth Pairing Request" */
                sec_req_ms = HAL_GetTick();
            }
            step = S_WAIT_PAIR;
            wait_since_ms = HAL_GetTick();
        } else if (error_code || att_error) {
            fail(error_code);
        } else {
            go(S_REG_PLAYER);
        }
        break;
    case S_REG_PLAYER:
        if (error_code || att_error) fail(error_code); else go(S_REG_TRACK);
        break;
    case S_REG_TRACK:
        if (error_code || att_error) fail(error_code);
        else { step = S_READY; pair_code = 0; info.version++; }
        break;
    default: break;  /* S_READY: a remote command finished */
    }
}

/* one Entity Update notification: [EntityID][AttributeID][Flags][value] */
static void on_entity_update(const uint8_t *d, uint8_t n)
{
    if (n < 3) return;
    uint8_t ent = d[0], attr = d[1], trunc = d[2] & 1;
    const uint8_t *v = d + 3;
    uint8_t len = n - 3;

    if (ent == ENT_PLAYER && attr == PL_NAME) {
        copy_str(info.app, sizeof info.app, v, len, trunc);
        if (len == 0) clear_info();                 /* no player app any more */
    } else if (ent == ENT_PLAYER && attr == PL_PLAYBACK) {
        char s[32];                                 /* "state,rate,elapsed", e.g. "1,1.0,42.5" */
        copy_str(s, sizeof s, v, len, 0);
        const char *p = s;
        info.play_state = (uint8_t)parse_float(&p);
        if (*p == ',') { p++; rate = parse_float(&p); }
        if (*p == ',') { p++; elapsed_s = parse_float(&p); }
        elapsed_tick = HAL_GetTick();
    } else if (ent == ENT_TRACK && attr == TR_TITLE) {
        copy_str(info.title, sizeof info.title, v, len, trunc);
    } else if (ent == ENT_TRACK && attr == TR_ARTIST) {
        copy_str(info.artist, sizeof info.artist, v, len, trunc);
    } else if (ent == ENT_TRACK && attr == TR_DURATION) {
        char s[16];
        copy_str(s, sizeof s, v, len, 0);
        const char *p = s;
        info.duration_s = parse_float(&p);
    }
    info.version++;
}

/* ---------------- BLE events (GATT/ATT group) ---------------- */
static SVCCTL_EvtAckStatus_t Ams_EventHandler(void *p_Event)
{
    hci_event_pckt *p_event_pckt = (hci_event_pckt *)(((hci_uart_pckt *)p_Event)->data);
    if (p_event_pckt->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE)
        return SVCCTL_EvtNotAck;
    evt_blecore_aci *e = (evt_blecore_aci *)p_event_pckt->data;

    switch (e->ecode) {
    case ACI_ATT_FIND_BY_TYPE_VALUE_RESP_VSEVT_CODE: {       /* service found */
        aci_att_find_by_type_value_resp_event_rp0 *r = (void *)e->data;
        if (r->Connection_Handle != conn || step != S_DISC_SVC) break;
        if (r->Num_of_Handle_Pair > 0) {
            svc_start = r->Attribute_Group_Handle_Pair[0].Found_Attribute_Handle;
            svc_end   = r->Attribute_Group_Handle_Pair[0].Group_End_Handle;
        }
        return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_ATT_READ_BY_TYPE_RESP_VSEVT_CODE: {             /* characteristics */
        aci_att_read_by_type_resp_event_rp0 *r = (void *)e->data;
        if (r->Connection_Handle != conn || step != S_DISC_CHAR) break;
        /* each entry: decl handle(2) properties(1) value handle(2) UUID(16) = 21 bytes */
        if (r->Handle_Value_Pair_Length == 21) {
            for (uint8_t i = 0; i + 21 <= r->Data_Length; i += 21) {
                const uint8_t *t = &r->Handle_Value_Pair_Data[i];
                uint16_t vh = (uint16_t)(t[3] | (t[4] << 8));
                if (memcmp(t + 5, UUID_EU, 16) == 0) eu_handle = vh;
                if (memcmp(t + 5, UUID_RC, 16) == 0) rc_handle = vh;
            }
        }
        return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_ATT_FIND_INFO_RESP_VSEVT_CODE: {                /* descriptors */
        aci_att_find_info_resp_event_rp0 *r = (void *)e->data;
        if (r->Connection_Handle != conn || step != S_DISC_DESC) break;
        if (r->Format == 1) {                                /* 16-bit UUIDs: handle(2) uuid(2) */
            for (uint8_t i = 0; i + 4 <= r->Event_Data_Length; i += 4) {
                const uint8_t *t = &r->Handle_UUID_Pair[i];
                uint16_t h = (uint16_t)(t[0] | (t[1] << 8)), u = (uint16_t)(t[2] | (t[3] << 8));
                /* first CCCD after the Entity Update value */
                if (u == 0x2902 && h > eu_handle && (!eu_cccd || h < eu_cccd)) eu_cccd = h;
            }
        }
        return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_ERROR_RESP_VSEVT_CODE: {                   /* phone said no */
        aci_gatt_error_resp_event_rp0 *r = (void *)e->data;
        if (r->Connection_Handle != conn || !proc_busy) break;
        att_error = r->Error_Code;
        return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_PROC_COMPLETE_VSEVT_CODE: {
        aci_gatt_proc_complete_event_rp0 *r = (void *)e->data;
        if (r->Connection_Handle != conn || !proc_busy) break;
        proc_done(r->Error_Code);
        return SVCCTL_EvtAckFlowEnable;
    }
    case ACI_GATT_NOTIFICATION_VSEVT_CODE: {
        aci_gatt_notification_event_rp0 *r = (void *)e->data;
        if (r->Connection_Handle != conn || r->Attribute_Handle != eu_handle || !eu_handle) break;
        on_entity_update(r->Attribute_Value, r->Attribute_Value_Length);
        return SVCCTL_EvtAckFlowEnable;
    }
    default: break;
    }
    return SVCCTL_EvtNotAck;
}

/* ---------------- public ---------------- */
void ams_client_init(void)
{
    SVCCTL_RegisterSvcHandler(Ams_EventHandler);
}

void ams_client_poll(void)
{
    uint32_t now = HAL_GetTick();

    /* BLE_TFT_AutoLock: two links now - follow the iPhone link only (found by ANCS in presence.c) */
    if (ble_ui_conn_handle() == 0xFFFF || (step != S_IDLE && ble_ui_conn_handle() != conn)) {   /* phone gone: start over */
        if (step != S_IDLE) { step = S_IDLE; proc_busy = 0; conn = 0xFFFF; pair_code = 0; sec_req_ms = 0; clear_info(); }
        return;
    }
    if (step == S_IDLE) {                                    /* just connected */
        conn = ble_ui_conn_handle();
        step = S_WAIT;
        next_try_ms = now + 1500;      /* let the phone finish its own setup (MTU, discovery) */
        return;
    }
    if (proc_busy) return;

    if (step == S_WAIT && (int32_t)(now - next_try_ms) >= 0) { go(S_DISC_SVC); return; }

    if (step == S_WAIT_PAIR) {
        /* retry the subscription every 3 s – also covers pairing done without our hook */
        if (now - wait_since_ms > 3000) { wait_since_ms = now; step = S_SUBSCRIBE; do_step(); }
        return;
    }
    if (next_try_ms && (int32_t)(now - next_try_ms) >= 0 &&
        (step == S_DISC_SVC || step == S_DISC_CHAR || step == S_SUBSCRIBE ||
         step == S_REG_PLAYER || step == S_REG_TRACK)) {
        next_try_ms = 0;
        do_step();                                           /* retry a command that failed */
        return;
    }
    if (step == S_READY && cmd_pending >= 0 && rc_handle) {
        uint8_t c = (uint8_t)cmd_pending;
        if (write_value(rc_handle, &c, 1)) cmd_pending = -1;
    }
}

ams_state_t ams_client_state(void)
{
    switch (step) {
    case S_IDLE:      return AMS_ST_NO_PHONE;
    case S_WAIT_PAIR: return AMS_ST_PAIRING;
    case S_READY:     return AMS_ST_READY;
    case S_NOT_FOUND: return AMS_ST_NOT_FOUND;
    case S_ERROR:     return AMS_ST_ERROR;
    default:          return AMS_ST_SEARCHING;
    }
}

const ams_info_t *ams_client_info(void) { return &info; }

float ams_client_elapsed_s(void)
{
    float t = elapsed_s;
    if (info.play_state == AMS_PLAY_PLAYING)
        t += rate * (float)(HAL_GetTick() - elapsed_tick) / 1000.0f;
    if (info.duration_s > 0 && t > info.duration_s) t = info.duration_s;
    return t < 0 ? 0 : t;
}

void ams_client_command(uint8_t cmd) { if (step == S_READY) cmd_pending = cmd; }

/* "Pair" button: some iPhones only show AMS after pairing – ask to pair, then search again */
void ams_client_pair(void)
{
    if (ble_ui_get_state() != BLE_UI_CONNECTED || proc_busy) return;
    aci_gap_peripheral_security_req(conn);
    sec_req_ms = HAL_GetTick();
    d_step = d_api = d_proc = d_att = 0;
    step = S_WAIT;
    next_try_ms = HAL_GetTick() + 4000;
}

uint32_t ams_client_pair_code(void) { return pair_code; }

void ams_on_numeric_comparison(uint32_t value) { pair_code = value; info.version++; }

void ams_on_pairing_complete(uint8_t status)
{
    pair_code = 0;
    if (status == 0 && step == S_WAIT_PAIR && !proc_busy) { step = S_SUBSCRIBE; do_step(); }
    info.version++;
}

/* e.g. "s06 a00 p41 t03" = step, API status, procedure error, ATT error (hex) – which step failed and the BLE error codes */
void ams_client_diag(char *buf, uint32_t size)
{
    static const char hex[] = "0123456789ABCDEF";
    const uint8_t v[4] = { d_step, d_api, d_proc, d_att };
    static const char *const name[4] = { "s", " a", " p", " t" };
    uint32_t n = 0;
    for (int i = 0; i < 4; i++) {
        for (const char *p = name[i]; *p && n + 1 < size; p++) buf[n++] = *p;
        if (n + 2 < size) { buf[n++] = hex[v[i] >> 4]; buf[n++] = hex[v[i] & 15]; }
    }
    buf[n] = '\0';
}
