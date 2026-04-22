/**
 * TOTP demo entry point for the N32WB031.
 *
 *   - BLE stack bring-up comes FIRST and is given a few ms of scheduling so
 *     it can run its async init state machine (enables LSI, configures sleep
 *     clock). Touching LSI/RTC before that sequence finishes tended to
 *     deadlock advertising.
 *   - Only after that do we enable the RTC domain and program the wake-up
 *     timer for the 1 Hz TOTP tick.
 *
 * Phone protocol (RDTSS characteristic — see app_totp_ble.h):
 *   0x01 [len][key...]         seed secret
 *   0x02 [8 bytes BE time]     seed Unix epoch
 *   0x03 [len][key...][time]   seed both in one write
 */

#include "n32wb03x.h"
#include "rwip.h"
#include "ns_ble.h"
#include "ns_log.h"
#include "ns_delay.h"

#include "FreeRTOS.h"
#include "task.h"

#include "main.h"
#include "app_ble.h"
#include "app_rtc.h"
#include "app_totp.h"
#include "app_tasks.h"
#include "app/app_gui.h"
#include "lcd.h"
#include "xpt2046.h"
#include "dfu_delay.h"

extern volatile uint8_t spi_dma_tx_complete;

TIM_TimeBaseInitType TIM_TimeBaseStructure;

/* Exposed to gui_task in app_tasks.c so both compilation units share the
 * same screen root object. */
Obj_Image main_screen;

static void TIM_Configuration(void)
{
    TIM_TimeBaseStructure.Period    = 10 - 1;
    TIM_TimeBaseStructure.Prescaler = 640 - 1;
    TIM_TimeBaseStructure.ClkDiv    = TIM_CLK_DIV1;
    TIM_TimeBaseStructure.CntMode   = TIM_CNT_MODE_UP;
    TIM_TimeBaseStructure.RepetCnt  = 0;

    TIM_InitTimeBase(TIM3, &TIM_TimeBaseStructure);
    TIM_ConfigInt(TIM3, TIM_INT_UPDATE, ENABLE);
    TIM_Enable(TIM3, ENABLE);
}

static void RCC_Configuration(void)
{
    RCC_EnableAPB1PeriphClk(RCC_APB1_PERIPH_TIM3, ENABLE);
}

static void NVIC_Configuration(void)
{
    NVIC_InitType NVIC_InitStructure;

    NVIC_InitStructure.NVIC_IRQChannel         = TIM3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel         = SPI_DMA_TX_IRQ;
    NVIC_InitStructure.NVIC_IRQChannelPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd      = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

int main(void)
{
    /* Hold the SWD lines long enough for a debugger to interrupt us. */
    delay_n_10us(200 * 1000);

		ns_log_usart_init();

    /* Boot banner on USART2 (PB4 TX @ 115200 8N1). Re-appearing without a
     * power cycle means the MCU restarted; the log lines above it will
     * pinpoint where. */
    printf("\r\n\r\n========== N32 TOTP boot ==========\r\n");

    /* BLE first — nothing else may touch LSI/RCC until the stack has finished
     * its own low-speed-clock configuration. */
    printf("[boot] app_ble_init\r\n");
    app_ble_init();
    printf("[boot] rf_tx_power_set\r\n");
    rf_tx_power_set(TX_POWER_MAX_VAL);

    printf("[boot] RCC/NVIC/TIM\r\n");
    RCC_Configuration();
    NVIC_Configuration();
    TIM_Configuration();

    /* RTC: enable the domain clock, program prescalers + 1 Hz wake-up.
     * BLE has already brought LSI up by this point. */
    printf("[boot] app_rtc_init\r\n");
    app_rtc_init();

    /* Front-panel keys (KEY1/KEY2/KEY3 on EXTI4_12). Must come AFTER BLE
     * init — ns_ble_stack_vtor_init captures the user's EXTI4_12 handler
     * from the vector table and dispatches to it via user_EXTI4_12_IRQHandler.
     * Configuring the EXTI lines before BLE runs its vector capture works
     * too, but this order keeps all IRQ wiring sequential in one place. */
    printf("[boot] keys\r\n");
    app_key_configuration();

    printf("[boot] LCD\r\n");
    LCD_Init();
    LCD_Display_Dir(USE_LCM_DIR);
    LCD_Clear(BLACK);

    printf("[boot] SCGUI\r\n");
    SC_GUI_Init((void *)LCD_Fast_DrawPoint, C_WHITE, C_BLUE, C_BLACK,
                (lv_font_t *)&lv_font_12);
    SC_Clear(0, 0, LCD_SCREEN_WIDTH - 1, LCD_SCREEN_HEIGHT - 1);
    sc_create_screen(NULL, (Obj_t *)&main_screen, 0, 0,
                     LCD_SCREEN_WIDTH, LCD_SCREEN_HEIGHT, ALIGN_NONE);

    printf("[boot] app_gui_init\r\n");
    app_gui_init(&main_screen);
    sc_create_task(0, app_gui_task, 250);

    /* ================== FreeRTOS kernel bring-up ==================
     * All three application tasks are created statically right here so
     * the control flow is obvious: no hidden helper, no dynamic heap.
     * The TCB/stack buffers live in app_tasks.c as static globals. */
    printf("[boot] xTaskCreateStatic ble_task\r\n");
    g_ble_task_handle = xTaskCreateStatic(
        ble_task, "ble",  BLE_TASK_STACK_WORDS,  NULL, 3,
        g_ble_stack,  &g_ble_tcb);

    printf("[boot] xTaskCreateStatic gui_task\r\n");
    g_gui_task_handle = xTaskCreateStatic(
        gui_task, "gui",  GUI_TASK_STACK_WORDS,  NULL, 2,
        g_gui_stack,  &g_gui_tcb);

    printf("[boot] xTaskCreateStatic totp_task\r\n");
    g_totp_task_handle = xTaskCreateStatic(
        totp_task, "totp", TOTP_TASK_STACK_WORDS, NULL, 1,
        g_totp_stack, &g_totp_tcb);

    if (g_ble_task_handle == NULL ||
        g_gui_task_handle == NULL ||
        g_totp_task_handle == NULL)
    {
        printf("[boot] xTaskCreateStatic FAILED "
               "(ble=%p gui=%p totp=%p)\r\n",
               (void *)g_ble_task_handle,
               (void *)g_gui_task_handle,
               (void *)g_totp_task_handle);
        for (;;) { }
    }

    /* Hand control to the scheduler — enables SysTick + PendSV and does
     * not return on success. */
    printf("[boot] vTaskStartScheduler\r\n");
    vTaskStartScheduler();

    printf("[boot] scheduler returned unexpectedly\r\n");
    for (;;) { }
}

/* ns_sleep hooks — we don't sleep yet, keep them empty. */
void app_sleep_prepare_proc(void) {}
void app_sleep_resume_proc(void)  {}
