/*****************************************************************************
 * TOTP demo — BLE bootstrap (mirrors the N32WB03x rdtss demo, minimal
 * adaptations for our DIS + custom TOTP-write profile).
 *****************************************************************************/
#include <string.h>

#include "n32wb03x.h"
#include "gapm_task.h"
#include "ns_sec.h"
#include "ns_ble.h"
#include "ns_log.h"

#include "app_ble.h"
#include "app_user_config.h"
#include "app_dis.h"
#include "app_profile/app_totp_ble.h"

static void app_ble_connected(void);
static void app_ble_disconnected(void);

/**
 * @brief user message handler
 */
void app_user_msg_handler(ke_msg_id_t const msgid, void const *p_param)
{
    (void)msgid;
    (void)p_param;
}

/**
 * @brief ble message handler
 */
void app_ble_msg_handler(struct ble_msg_t const *p_ble_msg)
{
    switch (p_ble_msg->msg_id)
    {
        case APP_BLE_OS_READY:
            NS_LOG_INFO("APP_BLE_OS_READY\r\n");
            break;
        case APP_BLE_GAP_CONNECTED:
            app_ble_connected();
            break;
        case APP_BLE_GAP_DISCONNECTED:
            app_ble_disconnected();
            break;
        default:
            break;
    }
}

/**
 * @brief advertising state-change handler
 */
void app_ble_adv_msg_handler(enum app_adv_mode adv_mode)
{
    (void)adv_mode;
}

/**
 * @brief BLE GAP initialization
 */
void app_ble_gap_params_init(void)
{
    struct ns_gap_params_t dev_info = {0};
    uint8_t *p_mac = SystemGetMacAddr();

    if (p_mac != NULL)
    {
        memcpy(dev_info.mac_addr.addr, p_mac, BD_ADDR_LEN);
    }
    else
    {
        memcpy(dev_info.mac_addr.addr, "\x01\x02\x03\x04\x05\x06", BD_ADDR_LEN);
    }

    dev_info.mac_addr_type = GAPM_STATIC_ADDR;
    dev_info.appearance    = 0;
    dev_info.dev_role      = GAP_ROLE_PERIPHERAL;

    dev_info.dev_name_len = sizeof(CUSTOM_DEVICE_NAME) - 1;
    memcpy(dev_info.dev_name, CUSTOM_DEVICE_NAME, dev_info.dev_name_len);

    dev_info.dev_conn_param.intv_min = MSECS_TO_UNIT(MIN_CONN_INTERVAL, MSECS_UNIT_1_25_MS);
    dev_info.dev_conn_param.intv_max = MSECS_TO_UNIT(MAX_CONN_INTERVAL, MSECS_UNIT_1_25_MS);
    dev_info.dev_conn_param.latency  = SLAVE_LATENCY;
    dev_info.dev_conn_param.time_out = MSECS_TO_UNIT(CONN_SUP_TIMEOUT, MSECS_UNIT_10_MS);
    dev_info.conn_param_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;

    ns_ble_gap_init(&dev_info);
}

/**
 * @brief BLE advertising initialization
 */
void app_ble_adv_init(void)
{
    struct ns_adv_params_t user_adv = {0};

    user_adv.adv_data_len = CUSTOM_USER_ADVERTISE_DATA_LEN;
    memcpy(user_adv.adv_data, CUSTOM_USER_ADVERTISE_DATA, CUSTOM_USER_ADVERTISE_DATA_LEN);
    user_adv.scan_rsp_data_len = CUSTOM_USER_ADV_SCNRSP_DATA_LEN;
    memcpy(user_adv.scan_rsp_data, CUSTOM_USER_ADV_SCNRSP_DATA, CUSTOM_USER_ADV_SCNRSP_DATA_LEN);

    user_adv.attach_appearance = false;
    user_adv.attach_name       = true;
    user_adv.ex_adv_enable     = false;
    user_adv.adv_phy           = PHY_1MBPS_VALUE;
    /* beacon_enable left at zero (= false): connectable-undirected advertising. */

    user_adv.directed_adv.enable = false;

    user_adv.fast_adv.enable   = true;
    user_adv.fast_adv.duration = CUSTOM_ADV_FAST_DURATION;
    user_adv.fast_adv.adv_intv = CUSTOM_ADV_FAST_INTERVAL;

    user_adv.slow_adv.enable   = true;
    user_adv.slow_adv.duration = CUSTOM_ADV_SLOW_DURATION;
    user_adv.slow_adv.adv_intv = CUSTOM_ADV_SLOW_INTERVAL;

    user_adv.ble_adv_msg_handler = app_ble_adv_msg_handler;

    ns_ble_adv_init(&user_adv);
}

void app_ble_sec_init(void)
{
    struct ns_sec_init_t sec_init = {0};

    sec_init.rand_pin_enable = false;
    sec_init.pin_code        = 123456;

    sec_init.pairing_feat.auth      = (SEC_PARAM_BOND | (SEC_PARAM_MITM << 2)
                                       | (SEC_PARAM_LESC << 3) | (SEC_PARAM_KEYPRESS << 4));
    sec_init.pairing_feat.iocap     = SEC_PARAM_IO_CAPABILITIES;
    sec_init.pairing_feat.key_size  = SEC_PARAM_KEY_SIZE;
    sec_init.pairing_feat.oob       = SEC_PARAM_OOB;
    sec_init.pairing_feat.ikey_dist = SEC_PARAM_IKEY;
    sec_init.pairing_feat.rkey_dist = SEC_PARAM_RKEY;
    sec_init.pairing_feat.sec_req   = SEC_PARAM_SEC_MODE_LEVEL;

    sec_init.bond_enable     = BOND_STORE_ENABLE;
    sec_init.bond_db_addr    = BOND_DATA_BASE_ADDR;
    sec_init.bond_max_peer   = MAX_BOND_PEER;
    sec_init.bond_sync_delay = 2000;

    sec_init.ns_sec_msg_handler = NULL;

    ns_sec_init(&sec_init);
}

void app_ble_prf_init(void)
{
#if (BLE_APP_DIS)
    ns_ble_add_prf_func_register(app_dis_add_dis);
#endif
    ns_ble_add_prf_func_register(app_totp_ble_add_service);
}

/**
 * @brief BLE initialization — entry point called from main().
 */
void app_ble_init(void)
{
    struct ns_stack_cfg_t app_handler = {0};
    app_handler.ble_msg_handler  = app_ble_msg_handler;
    app_handler.user_msg_handler = app_user_msg_handler;
    app_handler.lsc_cfg          = BLE_LSC_LSI_32000HZ;
    ns_ble_stack_init(&app_handler);

    app_ble_gap_params_init();
    app_ble_sec_init();
    app_ble_adv_init();
    app_ble_prf_init();

    ns_ble_adv_start();
}

static void app_ble_connected(void)
{
    NS_LOG_INFO("app_ble_connected: requesting MTU=247\r\n");
    app_totp_ble_set_connected(1);

    /* Proactively negotiate a larger ATT MTU. Default is 23 (20 B payload)
     * which can't fit our SET_BOTH command for any non-trivial key. Phones
     * generally accept whatever the peripheral requests up to their own cap
     * (usually 247 on Android, 185 on iOS). The stack also raises the LL
     * data-length limit for us. */
    ns_ble_mtu_set(247);
}

static void app_ble_disconnected(void)
{
    NS_LOG_INFO("app_ble_disconnected: restarting adv\r\n");
    app_totp_ble_set_connected(0);
    ns_ble_adv_start();
}
