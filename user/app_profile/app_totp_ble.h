#ifndef __APP_TOTP_BLE_H__
#define __APP_TOTP_BLE_H__

#include <stdint.h>
#include "rwip_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Wire format (over our write characteristic) ------------------------
 * Byte 0 = opcode, remaining bytes = payload.
 *   0x01 : SET_KEY   [len][key bytes (<= TOTP_KEY_MAX)]
 *   0x02 : SET_TIME  [8 bytes big-endian uint64 Unix seconds — MUST be UTC]
 *   0x03 : SET_BOTH  [len][key bytes][8 bytes BE UTC time]
 *   0x04 : SET_TZ    [4 bytes big-endian int32 offset from UTC, in seconds]
 *            +28800 (0x00007080) → UTC+8 (Beijing / CST)
 *            -18000 (0xFFFFB9B0) → UTC-5 (EST)
 *            Default is 0 (display UTC). TOTP itself always uses UTC.
 * Any other opcode is ignored.
 */
#define TOTP_BLE_CMD_SET_KEY   0x01u
#define TOTP_BLE_CMD_SET_TIME  0x02u
#define TOTP_BLE_CMD_SET_BOTH  0x03u
#define TOTP_BLE_CMD_SET_TZ    0x04u

/* Register RDTSS profile and handler with the BLE stack. */
void app_totp_ble_add_service(void);

/* Accessors for the current secret. Returns 0-length if no key yet. */
const uint8_t *app_totp_ble_get_key(void);
uint32_t       app_totp_ble_get_keylen(void);

/* Offset (seconds) added to RTC Unix time before display. TOTP math is
 * NEVER adjusted by this — only the HH:MM:SS clock is. */
int32_t        app_totp_ble_get_tz_offset(void);

/* 1 when a phone is currently connected. */
uint8_t        app_totp_ble_is_connected(void);

/* Hook called from app_ble.c on connection state change. */
void app_totp_ble_set_connected(uint8_t connected);

/* Optional: send arbitrary bytes back to the phone over the notify char —
 * returns 0 if the phone hasn't enabled notifications yet. */
uint8_t app_totp_ble_notify(const uint8_t *data, uint16_t length);

/* ---- Debug introspection ----------------------------------------------- */
#define TOTP_BLE_RX_SNAPSHOT_MAX   16u

uint32_t       app_totp_ble_rx_count(void);
uint16_t       app_totp_ble_last_rx_len(void);
const uint8_t *app_totp_ble_last_rx_data(void);
uint16_t       app_totp_ble_last_rx_handle(void);
uint32_t       app_totp_ble_add_count(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TOTP_BLE_H__ */
