/**
 * LSI-driven RTC for the TOTP demo.
 *
 * The BLE stack (ns_ble_stack_init → ns_ble_lsc_config) already enables LSI
 * and selects it via LSXSEL. We just need to enable the RTC domain clock and
 * program the prescalers + wake-up timer — touching LSI / LSXSEL again can
 * disturb the BLE baseband's low-speed timebase and kill advertising.
 *
 * Strategy: divide the 32 kHz LSI down to a 1 Hz CK_SPRE via the prescalers,
 * and use the RTC wake-up timer driven from CK_SPRE_16BITS to get a 1 Hz
 * interrupt. In the handler we just bump a 64-bit Unix-epoch counter.
 */
#include "app_rtc.h"
#include "app_totp.h"
#include "n32wb03x.h"
#include "n32wb03x_rcc.h"
#include "n32wb03x_rtc.h"
#include "n32wb03x_exti.h"
#include "misc.h"

static volatile uint64_t g_unix_time = 0;
static volatile uint8_t  g_time_set  = 0;

static void rtc_nvic_enable(void)
{
    EXTI_InitType    exti;
    NVIC_InitType    nvic;

    EXTI_ClrITPendBit(EXTI_LINE9);
    exti.EXTI_Line    = EXTI_LINE9;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_InitPeripheral(&exti);

    nvic.NVIC_IRQChannel         = RTC_IRQn;
    nvic.NVIC_IRQChannelPriority = 3;  /* lower than BLE */
    nvic.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&nvic);
}

void app_rtc_init(void)
{
    RTC_InitType rtc_init;

    /* BLE stack has already turned on LSI and pointed LSXSEL to it; only
     * bring up the APB slice for the PWR controller (some parts gate RTC
     * behind this) and enable the RTC domain clock. */
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_PWR, ENABLE);
    RCC_EnableRtcClk(ENABLE);
    RTC_WaitForSynchro();

    /* LSI ≈ 32 kHz → (127+1) × (255+1) = 32768, close enough to 1 Hz CK_SPRE
     * for a TOTP demo (drift is tolerable; the phone re-syncs at will). */
    rtc_init.RTC_HourFormat   = RTC_24HOUR_FORMAT;
    rtc_init.RTC_AsynchPrediv = 0x7F;
    rtc_init.RTC_SynchPrediv  = 0xFF;
    RTC_Init(&rtc_init);

    /* Wake-up timer fires on every CK_SPRE (≈1 Hz). */
    RTC_ConfigInt(RTC_INT_WUT, DISABLE);
    RTC_EnableWakeUp(DISABLE);
    RTC_ConfigWakeUpClock(RTC_WKUPCLK_CK_SPRE_16BITS);
    RTC_SetWakeUpCounter(0);

    rtc_nvic_enable();

    RTC_ConfigInt(RTC_INT_WUT, ENABLE);
    RTC_EnableWakeUp(ENABLE);
}

void app_rtc_set_unix(uint64_t unix_time)
{
    __disable_irq();
    g_unix_time = unix_time;
    g_time_set  = 1;
    __enable_irq();
}

uint64_t app_rtc_get_unix(void)
{
    uint64_t t;
    __disable_irq();
    t = g_unix_time;
    __enable_irq();
    return t;
}

uint8_t app_rtc_seconds_in_step(void)
{
    return (uint8_t)(app_rtc_get_unix() % TOTP_STEP);
}

uint8_t app_rtc_is_time_set(void)
{
    return g_time_set;
}

void app_rtc_on_wakeup(void)
{
    g_unix_time++;
}
