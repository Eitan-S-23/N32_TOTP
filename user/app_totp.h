#ifndef __APP_TOTP_H__
#define __APP_TOTP_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TOTP_DIGITS     6
#define TOTP_STEP       30u
#define TOTP_KEY_MAX    64u

/* TOTP (RFC 6238) with HMAC-SHA1, 30-second step, 6 digits. */
uint32_t totp_compute(const uint8_t *key, uint32_t keylen, uint64_t unix_time);

/* Counter = floor(unix_time / TOTP_STEP). */
uint64_t totp_counter_of(uint64_t unix_time);

/* HMAC-SHA1 primitive (exposed for testing). */
void hmac_sha1(const uint8_t *key, uint32_t keylen,
               const uint8_t *msg, uint32_t msglen,
               uint8_t out[20]);

/* Recompute the cached prev/curr/next token triple when the 30-second
 * counter has advanced. Intended to be called ~1 Hz from a dedicated
 * task (see user/app_tasks.c:totp_task); cheap (short-circuits) the
 * 29 out of 30 ticks that don't cross a counter boundary. */
void app_totp_refresh(void);

/* Atomically read the cached triple produced by app_totp_refresh().
 * Returns 1 when the cache is valid, 0 when no key or no time is set. */
int app_totp_snapshot(uint32_t *counter_lo,
                      uint32_t *prev,
                      uint32_t *curr,
                      uint32_t *next);

#ifdef __cplusplus
}
#endif

#endif /* __APP_TOTP_H__ */
