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

#ifdef __cplusplus
}
#endif

#endif /* __APP_TOTP_H__ */
