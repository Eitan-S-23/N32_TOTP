/**
 * TOTP command channel over BLE — structured exactly like the SDK's
 * projects/n32wb03x_EVAL/ble/rdtss/src/app_profile/app_rdtss.c demo:
 *   - 7-attribute RDTSS service: SVC, WRITE_CHAR, WRITE_VAL, WRITE_CFG
 *     (0x2901 user description), NTF_CHAR, NTF_VAL, NTF_CFG (CCC 0x2902).
 *   - Phone writes commands to WRITE_VAL (index 2).
 *   - Phone enables notifications by writing 0x0001 to NTF_CFG (index 6).
 *   - MCU notifies back on NTF_VAL (index 5) via app_totp_ble_notify().
 *
 * The earlier 3-attribute variant crashed on writes >2 bytes; mirroring the
 * SDK layout byte-for-byte sidesteps whatever the stack was unhappy with.
 */
#include "rwip_config.h"

#if (BLE_RDTSS_SERVER)

#include <string.h>

#include "app_totp_ble.h"
#include "app_totp.h"
#include "app_rtc.h"

#include "co_utils.h"
#include "gapm_task.h"
#include "ns_ble.h"
#include "ns_log.h"
#include "prf.h"
#include "ke_timer.h"
#include "rdtss.h"
#include "rdtss_task.h"

/* ---- UUIDs (128-bit, little-endian byte order in the table) -------------
 *   Service : 0000278F-08C2-11E1-9073-0E8AC7B41001
 *   Write   : 0000278F-08C2-11E1-9073-0E8AC7B40001
 *   Notify  : 0000278F-08C2-11E1-9073-0E8AC7B40002
 */
#define ATT_SERVICE_TOTP_128      {0x01,0x10,0xB4,0xC7,0x8A,0x0E, 0x73,0x90, 0xE1,0x11, 0xC2,0x08, 0x8F,0x27,0x00,0x00}
#define ATT_CHAR_TOTP_WRITE_128   {0x01,0x00,0xB4,0xC7,0x8A,0x0E, 0x73,0x90, 0xE1,0x11, 0xC2,0x08, 0x8F,0x27,0x00,0x00}
#define ATT_CHAR_TOTP_NTF_128     {0x02,0x00,0xB4,0xC7,0x8A,0x0E, 0x73,0x90, 0xE1,0x11, 0xC2,0x08, 0x8F,0x27,0x00,0x00}

/* Attribute indexes inside the service. Values MUST be in this order —
 * rdtss.c walks the table sequentially when creating the GATT DB. */
enum
{
    RDTSS_IDX_SVC,
    RDTSS_IDX_WRITE_CHAR,
    RDTSS_IDX_WRITE_VAL,
    RDTSS_IDX_WRITE_CFG,
    RDTSS_IDX_NTF_CHAR,
    RDTSS_IDX_NTF_VAL,
    RDTSS_IDX_NTF_CFG,
    RDTSS_IDX_NB,
};

static uint8_t notify_en = 0;

static const uint8_t totp_svc_uuid[16] = ATT_SERVICE_TOTP_128;

static const struct attm_desc_128 totp_att_db[RDTSS_IDX_NB] =
{
    /* Service declaration */
    [RDTSS_IDX_SVC]        = {{0x00,0x28}, PERM(RD, ENABLE),                                                0,                                          0     },

    /* Characteristic declaration for the write value — RD only, not writable.
     * (The earlier table copied the SDK sample's extra WRITE_REQ perm on the
     * declaration itself; some phone stacks will then probe-write the CCCD-
     * style prefix into the declaration handle, which has no path through
     * rdtss_task and trips the stack.) */
    [RDTSS_IDX_WRITE_CHAR] = {{0x03,0x28}, PERM(RD, ENABLE),                                                0,                                          0     },
    /* Write value — accept both WRITE_REQ (0x12) and WRITE_CMD (0x52). Some
     * phone BLE helpers default to Write Request for short payloads, and
     * without the WRITE_REQ perm bit the server rejects the PDU. */
    [RDTSS_IDX_WRITE_VAL]  = {ATT_CHAR_TOTP_WRITE_128,
                              PERM(WRITE_REQ, ENABLE) | PERM(WRITE_COMMAND, ENABLE),
                              PERM_VAL(UUID_LEN, 0x02),
                              128 },
    /* User description (kept for parity with the SDK demo) */
    [RDTSS_IDX_WRITE_CFG]  = {{0x01,0x29}, PERM(RD, ENABLE),                                                0,                                          0     },

    /* Characteristic declaration for the notify value */
    [RDTSS_IDX_NTF_CHAR]   = {{0x03,0x28}, PERM(RD, ENABLE),                                                0,                                          0     },
    /* Notify value */
    [RDTSS_IDX_NTF_VAL]    = {ATT_CHAR_TOTP_NTF_128,   PERM(NTF, ENABLE),                                   PERM_VAL(UUID_LEN, 0x02),                   64    },
    /* CCC — phone writes 0x0001 to enable notifications */
    [RDTSS_IDX_NTF_CFG]    = {{0x02,0x29}, PERM(RD, ENABLE) | PERM(WRITE_REQ, ENABLE),                      0,                                          0     },
};

/* ---- Shared state (read from GUI task, written from BLE task) ----------- */

static uint8_t  s_key[TOTP_KEY_MAX];
static uint32_t s_keylen    = 0;
static uint8_t  s_connected = 0;
static volatile int32_t s_tz_offset_seconds = 0;

static volatile uint32_t s_rx_count        = 0;
static volatile uint16_t s_last_rx_len     = 0;
static volatile uint16_t s_last_rx_handle  = 0xFFFF;
static volatile uint32_t s_add_count       = 0;
static uint8_t           s_last_rx[TOTP_BLE_RX_SNAPSHOT_MAX];

const uint8_t *app_totp_ble_get_key(void)        { return s_key; }
uint32_t       app_totp_ble_get_keylen(void)     { return s_keylen; }
uint8_t        app_totp_ble_is_connected(void)   { return s_connected; }
int32_t        app_totp_ble_get_tz_offset(void)  { return s_tz_offset_seconds; }

uint32_t       app_totp_ble_rx_count(void)       { return s_rx_count; }
uint16_t       app_totp_ble_last_rx_len(void)    { return s_last_rx_len; }
const uint8_t *app_totp_ble_last_rx_data(void)   { return s_last_rx; }
uint16_t       app_totp_ble_last_rx_handle(void) { return s_last_rx_handle; }
uint32_t       app_totp_ble_add_count(void)      { return s_add_count; }

void app_totp_ble_set_connected(uint8_t connected)
{
    s_connected = connected ? 1u : 0u;
    NS_LOG_INFO("totp_ble: %s\r\n", s_connected ? "CONNECTED" : "DISCONNECTED");
    if (!s_connected)
    {
        notify_en = 0;
    }
}

/* ---- Command decoding --------------------------------------------------- */

static void dump_hex(const uint8_t *buf, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++)
    {
        NS_LOG_INFO("%02X ", buf[i]);
    }
    NS_LOG_INFO("\r\n");
}

static void decode_cmd(const uint8_t *value, uint16_t length)
{
    uint8_t  klen;
    uint64_t t;
    uint32_t t_hi, t_lo;
    uint32_t tz_raw;
    unsigned i;

    if (length < 1)
    {
        NS_LOG_ERROR("decode_cmd: empty payload\r\n");
        return;
    }

    NS_LOG_INFO("decode_cmd: op=0x%02X len=%u\r\n", value[0], (unsigned)length);

    switch (value[0])
    {
        case TOTP_BLE_CMD_SET_KEY:
            if (length < 2) { NS_LOG_ERROR("SET_KEY: short\r\n"); return; }
            klen = value[1];
            if (klen == 0 || klen > TOTP_KEY_MAX)
            {
                NS_LOG_ERROR("SET_KEY: bad klen=%u (max %u)\r\n",
                             (unsigned)klen, (unsigned)TOTP_KEY_MAX);
                return;
            }
            if ((uint16_t)(2 + klen) > length)
            {
                NS_LOG_ERROR("SET_KEY: truncated (need %u, have %u)\r\n",
                             (unsigned)(2 + klen), (unsigned)length);
                return;
            }
            memcpy(s_key, &value[2], klen);
            s_keylen = klen;
            NS_LOG_INFO("SET_KEY ok klen=%u key=", (unsigned)klen);
            dump_hex(s_key, klen);
            break;

        case TOTP_BLE_CMD_SET_TIME:
            if (length < 9) { NS_LOG_ERROR("SET_TIME: short len=%u\r\n", (unsigned)length); return; }
            t = 0;
            for (i = 0; i < 8; i++)
            {
                t = (t << 8) | value[1 + i];
            }
            app_rtc_set_unix(t);
            t_hi = (uint32_t)(t >> 32);
            t_lo = (uint32_t)(t & 0xFFFFFFFFu);
            NS_LOG_INFO("SET_TIME ok unix=0x%08X%08X\r\n",
                        (unsigned)t_hi, (unsigned)t_lo);
            break;

        case TOTP_BLE_CMD_SET_BOTH:
            if (length < 2) { NS_LOG_ERROR("SET_BOTH: short\r\n"); return; }
            klen = value[1];
            if (klen == 0 || klen > TOTP_KEY_MAX)
            {
                NS_LOG_ERROR("SET_BOTH: bad klen=%u\r\n", (unsigned)klen);
                return;
            }
            if ((uint16_t)(2 + klen + 8) > length)
            {
                NS_LOG_ERROR("SET_BOTH: truncated (need %u, have %u)\r\n",
                             (unsigned)(2 + klen + 8), (unsigned)length);
                return;
            }
            memcpy(s_key, &value[2], klen);
            s_keylen = klen;
            t = 0;
            for (i = 0; i < 8; i++)
            {
                t = (t << 8) | value[2 + klen + i];
            }
            app_rtc_set_unix(t);
            t_hi = (uint32_t)(t >> 32);
            t_lo = (uint32_t)(t & 0xFFFFFFFFu);
            NS_LOG_INFO("SET_BOTH ok klen=%u unix=0x%08X%08X\r\n",
                        (unsigned)klen, (unsigned)t_hi, (unsigned)t_lo);
            break;

        case TOTP_BLE_CMD_SET_TZ:
            /* 4-byte big-endian signed offset. Parse as unsigned, cast to
             * int32_t so the sign bit propagates correctly. */
            if (length < 5) { NS_LOG_ERROR("SET_TZ: short len=%u\r\n", (unsigned)length); return; }
            tz_raw = 0;
            for (i = 0; i < 4; i++)
            {
                tz_raw = (tz_raw << 8) | value[1 + i];
            }
            s_tz_offset_seconds = (int32_t)tz_raw;
            NS_LOG_INFO("SET_TZ ok offset=%d s\r\n", (int)s_tz_offset_seconds);
            break;

        default:
            NS_LOG_ERROR("decode_cmd: unknown opcode 0x%02X\r\n", value[0]);
            break;
    }
}

/* ---- RDTSS callbacks (structure matches SDK app_rdtss.c) ---------------- */

static int rdtss_value_req_ind_handler(ke_msg_id_t const msgid,
                                       struct rdtss_value_req_ind const *req_value,
                                       ke_task_id_t const dest_id,
                                       ke_task_id_t const src_id)
{
    NS_LOG_INFO("rdtss_value_req_ind: conidx=%u att_idx=%u\r\n",
                (unsigned)req_value->conidx, (unsigned)req_value->att_idx);

    /* Phone read — no meaningful value to return, just acknowledge. */
    struct rdtss_value_req_rsp *rsp_value = KE_MSG_ALLOC_DYN(RDTSS_VALUE_REQ_RSP,
                                                              src_id, dest_id,
                                                              rdtss_value_req_rsp, 0);
    rsp_value->status  = 0;
    rsp_value->conidx  = req_value->conidx;
    rsp_value->length  = 0;
    rsp_value->att_idx = req_value->att_idx;
    ke_msg_send(rsp_value);
    return KE_MSG_CONSUMED;
}

static int rdtss_val_write_ind_handler(ke_msg_id_t const msgid,
                                       struct rdtss_val_write_ind const *ind_value,
                                       ke_task_id_t const dest_id,
                                       ke_task_id_t const src_id)
{
    uint16_t handle;
    uint16_t length;
    uint16_t snap_len;

    if (ind_value == NULL)
    {
        NS_LOG_ERROR("write_ind: NULL param!\r\n");
        return KE_MSG_CONSUMED;
    }

    handle = ind_value->handle;
    length = ind_value->length;

    NS_LOG_INFO("write_ind: handle=%u length=%u conidx=%u bytes=",
                (unsigned)handle, (unsigned)length, (unsigned)ind_value->conidx);
    dump_hex(ind_value->value, length);

    /* Snapshot for the on-screen debug row. */
    snap_len = length;
    if (snap_len > TOTP_BLE_RX_SNAPSHOT_MAX)
    {
        snap_len = TOTP_BLE_RX_SNAPSHOT_MAX;
    }
    if (snap_len > 0)
    {
        memcpy(s_last_rx, ind_value->value, snap_len);
    }
    s_last_rx_len    = length;
    s_last_rx_handle = handle;
    s_rx_count++;

    switch (handle)
    {
        case RDTSS_IDX_NTF_CFG:
            if (length == 2)
            {
                /* Little-endian CCC value: bit0 = notify, bit1 = indicate. */
                uint16_t cfg_value = ind_value->value[0] | ((uint16_t)ind_value->value[1] << 8);
                NS_LOG_INFO("CCCD write: cfg=0x%04X\r\n", (unsigned)cfg_value);
                if (cfg_value == PRF_CLI_START_NTF)
                {
                    notify_en = 1;
                    NS_LOG_INFO("notify enabled\r\n");
                }
                else if (cfg_value == PRF_CLI_STOP_NTFIND)
                {
                    notify_en = 0;
                    NS_LOG_INFO("notify disabled\r\n");
                }
            }
            else
            {
                NS_LOG_WARNING("CCCD write with length %u (expected 2)\r\n",
                               (unsigned)length);
            }
            break;

        case RDTSS_IDX_WRITE_VAL:
            decode_cmd(ind_value->value, length);
            break;

        default:
            NS_LOG_WARNING("write_ind on unexpected handle %u\r\n",
                           (unsigned)handle);
            break;
    }

    return KE_MSG_CONSUMED;
}

static int rdtss_val_ntf_cfm_handler(ke_msg_id_t const msgid,
                                     struct rdtss_val_ntf_cfm const *cfm_value,
                                     ke_task_id_t const dest_id,
                                     ke_task_id_t const src_id)
{
    (void)cfm_value;
    return KE_MSG_CONSUMED;
}

/* ---- Notify helper ------------------------------------------------------ */

uint8_t app_totp_ble_notify(const uint8_t *data, uint16_t length)
{
    struct rdtss_val_ntf_ind_req *req;

    if (!notify_en || data == NULL || length == 0 || !s_connected)
    {
        return 0;
    }

    req = KE_MSG_ALLOC_DYN(RDTSS_VAL_NTF_REQ,
                           prf_get_task_from_id(TASK_ID_RDTSS),
                           TASK_APP,
                           rdtss_val_ntf_ind_req,
                           length);
    if (req == NULL)
    {
        /* BLE heap exhausted — silently drop this notification rather than
         * dereferencing NULL and faulting. */
        return 0;
    }
    req->conidx       = app_env.conidx;
    req->notification = true;
    req->handle       = RDTSS_IDX_NTF_VAL;
    req->length       = length;
    memcpy(&req->value[0], data, length);
    ke_msg_send(req);
    return 1;
}

/* ---- Registration ------------------------------------------------------- */

static const struct ke_msg_handler app_totp_msg_handler_list[] =
{
    {RDTSS_VALUE_REQ_IND, (ke_msg_func_t)rdtss_value_req_ind_handler},
    {RDTSS_VAL_WRITE_IND, (ke_msg_func_t)rdtss_val_write_ind_handler},
    {RDTSS_VAL_NTF_CFM,   (ke_msg_func_t)rdtss_val_ntf_cfm_handler},
};

static const struct app_subtask_handlers app_totp_handlers = APP_HANDLERS(app_totp);

static void app_totp_ble_init(void)
{
    struct prf_task_t     prf;
    struct prf_get_func_t get_func;

    prf.prf_task_id      = TASK_ID_RDTSS;
    prf.prf_task_handler = &app_totp_handlers;
    ns_ble_prf_task_register(&prf);

    get_func.task_id          = TASK_ID_RDTSS;
    get_func.prf_itf_get_func = rdtss_prf_itf_get;
    prf_get_itf_func_register(&get_func);
}

void app_totp_ble_add_service(void)
{
    struct rdtss_db_cfg            *db_cfg;
    struct gapm_profile_task_add_cmd *req;

    s_add_count++;
    NS_LOG_INFO("totp_ble: add_service #%u (max_nb_att=%u)\r\n",
                (unsigned)s_add_count, (unsigned)RDTSS_IDX_NB);

    req = KE_MSG_ALLOC_DYN(GAPM_PROFILE_TASK_ADD_CMD,
                           TASK_GAPM, TASK_APP,
                           gapm_profile_task_add_cmd,
                           sizeof(struct rdtss_db_cfg));

    req->operation   = GAPM_PROFILE_TASK_ADD;
    req->sec_lvl     = PERM(SVC_AUTH, NO_AUTH);
    req->prf_task_id = TASK_ID_RDTSS;
    req->app_task    = TASK_APP;
    req->start_hdl   = 0;

    db_cfg             = (struct rdtss_db_cfg *)req->param;
    db_cfg->att_tbl    = &totp_att_db[0];
    db_cfg->svc_uuid   = &totp_svc_uuid[0];
    db_cfg->max_nb_att = RDTSS_IDX_NB;

    ke_msg_send(req);

    app_totp_ble_init();
}

#endif /* BLE_RDTSS_SERVER */
