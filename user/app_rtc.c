/**
 * LSI-driven RTC for the TOTP demo.
 *
 * The BLE stack (ns_ble_stack_init → ns_ble_lsc_config) already enables LSI
 * and selects it via LSXSEL. We just need to enable the RTC domain clock and
 * program the prescalers + wake-up timer — touching LSI / LSXSEL again can
 * disturb the BLE baseband's low-speed timebase and kill advertising.
 *
 * Frequency note: the N32WB03x SDK defines LSI_VALUE = 32000 Hz and the BLE
 * stack is initialised with BLE_LSC_LSI_32000HZ. The prescalers below divide
 * by exactly 32000 (125 × 256). Earlier revisions divided by 32768, which is
 * a 2.34 % (≈23,400 ppm) systematic error — far beyond what smooth
 * calibration can trim — and was the root cause of the bad drift.
 *
 * Residual drift is removed in two steps:
 *   1. The BLE stack measured the real LSI frequency at startup and stored
 *      it in g_lsi_1_syscle_cnt_value (HSI cycles per 1 LSI cycle, averaged
 *      over 126 LSI cycles with HSI=32 MHz; ideal value = 1000 for LSI=32 kHz).
 *   2. We translate the error to a ppm correction and feed it to the RTC's
 *      smooth calibration logic (range ≈ ±488 ppm, step ≈ 0.954 ppm).
 */
#include "app_rtc.h"
#include "app_totp.h"
#include "n32wb03x.h"
#include "n32wb03x_rcc.h"
#include "n32wb03x_rtc.h"
#include "n32wb03x_exti.h"
#include "misc.h"

/* Measured by the BLE stack during ns_ble_stack_init(); represents HSI
 * cycles per LSI cycle with HSI=32 MHz. Exactly 1000 when LSI is 32 kHz. */
extern uint32_t g_lsi_1_syscle_cnt_value;

static volatile uint64_t g_unix_time = 0;
static volatile uint8_t  g_time_set  = 0;

/* Cached CALP/CALM we most recently programmed — handy for diagnostics. */
static int32_t g_applied_calib_ppm = 0;

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

/* Smooth calibration on this MCU runs over a 2^20-RTCCLK window (≈32 s at
 * 32 kHz). Within that window CALP injects 512 extra pulses when set, and
 * CALM removes CALM pulses. Resulting frequency shift:
 *
 *   ppm = (CALP ? 512 : 0  -  CALM) * 1e6 / 2^20
 *       ≈ (CALP ? 512 : 0  -  CALM) * 0.9537
 *
 * Range: [-487.3 ppm, +488.3 ppm]. Step ≈ 0.954 ppm. */
#define CALIB_STEP_SCALE       (1 << 20)       /* 1,048,576 */

static void rtc_program_smooth_calib(int32_t ppm)
{
    uint32_t plus;
    int32_t  calm;

    /* Clamp to the calibrator's actual range (see comment block above). */
    if (ppm >  488) ppm =  488;
    if (ppm < -487) ppm = -487;

    if (ppm >= 0)
    {
        /* CALP=1 → nets +512 pulses, CALM subtracts; final pulse delta =
         * 512 - CALM. Solve CALM for the target ppm. Add half a step for
         * round-to-nearest. */
        plus = RTC_SMOOTH_CALIB_PLUS_PULSES_SET;
        calm = 512 - (int32_t)(((int64_t)ppm * CALIB_STEP_SCALE
                                 + (int64_t)500000) / 1000000);
    }
    else
    {
        /* CALP=0 → only CALM acts, subtracting CALM pulses. ppm is negative
         * so CALM must be positive. */
        plus = RTC_SMOOTH_CALIB_PLUS_PULSES_RESET;
        calm = (int32_t)(((int64_t)(-ppm) * CALIB_STEP_SCALE
                           + (int64_t)500000) / 1000000);
    }

    if (calm < 0)   calm = 0;
    if (calm > 511) calm = 511;

    RTC_ConfigSmoothCalib(SMOOTH_CALIB_32SEC, plus, (uint32_t)calm);
    g_applied_calib_ppm = ppm;
}

/* Read the BLE stack's LSI measurement and derive the ppm error vs the
 * 32 kHz target that our prescaler assumes. Returns 0 when no measurement
 * is available (e.g., BLE stack not yet initialised). */
static int32_t rtc_auto_calib_ppm(void)
{
    uint32_t cnt = g_lsi_1_syscle_cnt_value;
    if (cnt == 0) return 0;

    /* actual_lsi = 32,000,000 / cnt
     * ppm_err    = (actual_lsi / 32000 - 1) * 1e6
     *            = (1,000,000,000 / cnt) - 1,000,000
     * The RTC runs too *fast* when ppm_err > 0, so the correction is its
     * negation — we want CALM to remove pulses and slow the tick down. */
    int32_t ppm_err = (int32_t)(1000000000u / cnt) - 1000000;
    return -ppm_err;
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

    /* LSI target = 32000 Hz (see file header). (124+1) × (255+1) = 32000, so
     * ck_apre = 256 Hz and ck_spre = 1 Hz. Matches LSI_VALUE in the SDK and
     * BLE_LSC_LSI_32000HZ in the BLE stack init. */
    rtc_init.RTC_HourFormat   = RTC_24HOUR_FORMAT;
    rtc_init.RTC_AsynchPrediv = 0x7C;  /* DIVA = 124 → /125 */
    rtc_init.RTC_SynchPrediv  = 0xFF;  /* DIVS = 255 → /256 */
    RTC_Init(&rtc_init);

    /* Trim residual LSI error with the RTC's smooth calibration block.
     * Uses the frequency the BLE stack already measured at startup. */
    rtc_program_smooth_calib(rtc_auto_calib_ppm());

    /* Wake-up timer fires on every CK_SPRE (≈1 Hz). */
    RTC_ConfigInt(RTC_INT_WUT, DISABLE);
    RTC_EnableWakeUp(DISABLE);
    RTC_ConfigWakeUpClock(RTC_WKUPCLK_CK_SPRE_16BITS);
    RTC_SetWakeUpCounter(0);

    rtc_nvic_enable();

    RTC_ConfigInt(RTC_INT_WUT, ENABLE);
    RTC_EnableWakeUp(ENABLE);
}

void app_rtc_set_calibration_ppm(int32_t ppm)
{
    rtc_program_smooth_calib(ppm);
}

int32_t app_rtc_get_calibration_ppm(void)
{
    return g_applied_calib_ppm;
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
