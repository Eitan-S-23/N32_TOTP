#ifndef __APP_RTC_H__
#define __APP_RTC_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Configure LSI, route it to the RTC, set prescalers for a 1 Hz CK_SPRE, and
 * start a periodic wake-up that fires every second. */
void app_rtc_init(void);

/* Seed the epoch counter from a Unix timestamp (seconds since 1970-01-01). */
void app_rtc_set_unix(uint64_t unix_time);

/* Current epoch seconds. */
uint64_t app_rtc_get_unix(void);

/* 0..TOTP_STEP-1 — elapsed seconds inside the current 30 s window. */
uint8_t app_rtc_seconds_in_step(void);

/* 0 until app_rtc_set_unix has been called at least once. */
uint8_t app_rtc_is_time_set(void);

/* Called from RTC_IRQHandler in n32wb03x_it.c. */
void app_rtc_on_wakeup(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_RTC_H__ */
