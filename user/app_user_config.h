/*****************************************************************************
 * TOTP demo — app_user_config.h (mirrors N32WB03x rdtss demo's config)
 *****************************************************************************/

#ifndef _APP_USER_CONFIG_H_
#define _APP_USER_CONFIG_H_

#include "ns_adv_data_def.h"

/* Device name */
#define CUSTOM_DEVICE_NAME                  "N32-TOTP"

/* adv configer*/
#define CUSTOM_ADV_FAST_INTERVAL               160     /**< 100 ms (× 0.625 ms) */
#define CUSTOM_ADV_SLOW_INTERVAL               3200    /**< 2 s (× 0.625 ms)    */

#define CUSTOM_ADV_FAST_DURATION               0       /**< 0 = unlimited       */
#define CUSTOM_ADV_SLOW_DURATION               180     /**< 180 s               */

/* Advertising payload — same shape as the official rdtss demo:
 *   AD field: Service Data, 16-bit UUID = Device Information Service (0x180A)
 * The stack prepends the Flags AD automatically when disc_mode = GEN_DISC. */
#define CUSTOM_USER_ADVERTISE_DATA \
            "\x03"\
            ADV_TYPE_SERVICE_DATA_16BIT_UUID\
            ADV_UUID_DEVICE_INFORMATION_SERVICE

#define CUSTOM_USER_ADVERTISE_DATA_LEN (sizeof(CUSTOM_USER_ADVERTISE_DATA) - 1)

/* Scan response payload — Manufacturer Specific Data "NS TOTP" tag.
 * The stack additionally appends the complete local name when
 * attach_name = true. */
#define CUSTOM_USER_ADV_SCNRSP_DATA  \
            "\x0a"\
            ADV_TYPE_MANUFACTURER_SPECIFIC_DATA\
            "\xff\xffNations"

#define CUSTOM_USER_ADV_SCNRSP_DATA_LEN (sizeof(CUSTOM_USER_ADV_SCNRSP_DATA) - 1)


/*  connection config  */
#define MIN_CONN_INTERVAL                   15      /**< 15 ms  */
#define MAX_CONN_INTERVAL                   30      /**< 30 ms  */
#define SLAVE_LATENCY                       0
#define CONN_SUP_TIMEOUT                    5000    /**< 5 s    */
#define FIRST_CONN_PARAMS_UPDATE_DELAY      (5000)

/* security config — same as rdtss demo */
#define SEC_PARAM_IO_CAPABILITIES           GAP_IO_CAP_NO_INPUT_NO_OUTPUT
#define SEC_PARAM_OOB                       0
#define SEC_PARAM_KEY_SIZE                  16
#define SEC_PARAM_BOND                      1
#define SEC_PARAM_MITM                      1
#define SEC_PARAM_LESC                      0
#define SEC_PARAM_KEYPRESS                  0
#define SEC_PARAM_IKEY                      GAP_KDIST_NONE
#define SEC_PARAM_RKEY                      GAP_KDIST_ENCKEY
#define SEC_PARAM_SEC_MODE_LEVEL            GAP_NO_SEC

/* bond config */
#define MAX_BOND_PEER                       5
#define BOND_STORE_ENABLE                   0
#define BOND_DATA_BASE_ADDR                 0x0103B000

/* Profiles — DIS (read-only device info) + RDTSS (our write channel). */
#define CFG_APP_DIS       1
#define CFG_PRF_DISS      1
#define CFG_PRF_RDTSS     1

/* Logging */
#define NS_LOG_ERROR_ENABLE      1
#define NS_LOG_WARNING_ENABLE    1
#define NS_LOG_INFO_ENABLE       1
#define NS_LOG_DEBUG_ENABLE      1
#define NS_LOG_USART_ENABLE      1

#define NS_TIMER_ENABLE          0

#define FIRMWARE_VERSION         "1.0.0"
#define HARDWARE_VERSION         "1.0.0"

#endif /* _APP_USER_CONFIG_H_ */
