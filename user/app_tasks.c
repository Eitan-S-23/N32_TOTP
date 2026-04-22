/**
 * app_tasks.c — static task storage + task bodies for the FreeRTOS port.
 *
 *   ble_task  prio 3, 128w stack — cooperative BLE dispatch loop
 *   gui_task  prio 2, 128w stack — SCGUI loop + LCD repaint
 *   totp_task prio 1,  96w stack — recomputes the TOTP on every RTC
 *                                  1 Hz tick (notified from the ISR)
 *   idle      prio 0,  96w stack — provided by FreeRTOS, storage below
 *
 * The three task bodies and their StaticTask_t / StackType_t storage
 * are exposed via app_tasks.h so main() can call xTaskCreateStatic()
 * explicitly — no hidden wrapper indirection.
 */

#include "FreeRTOS.h"
#include "task.h"

#include "app_tasks.h"
#include "app_totp.h"
#include "rwip.h"

#include "sc_event_task.h"
#include "sc_gui.h"
#include "app/app_gui.h"

/* Forward decl of the screen root in main.c so gui_task can repaint it. */
extern Obj_Image main_screen;

/* ------------------------- static storage ------------------------- */

StaticTask_t g_ble_tcb;
StackType_t  g_ble_stack[BLE_TASK_STACK_WORDS];

StaticTask_t g_gui_tcb;
StackType_t  g_gui_stack[GUI_TASK_STACK_WORDS];

StaticTask_t g_totp_tcb;
StackType_t  g_totp_stack[TOTP_TASK_STACK_WORDS];

TaskHandle_t g_ble_task_handle;
TaskHandle_t g_gui_task_handle;
TaskHandle_t g_totp_task_handle;

/* ------------------------- task bodies ------------------------- */

void ble_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        /* BLE stack's cooperative event dispatch. The radio's hard timing
         * lives in IRQs (priority 0–1), not here, so a 1 ms cadence is
         * plenty to drain host-side events without starving the rest of
         * the system. */
        rwip_schedule();
        //vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void gui_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        sc_widget_draw_screen(&main_screen);    //裸机不需要
				sc_task_loop();
        //vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void totp_task(void *arg)
{
    (void)arg;
    for (;;)
    {
        /* RTC_IRQHandler gives this notification once per 1 Hz CK_SPRE
         * tick (see user/n32wb03x_it.c). */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        app_totp_refresh();
    }
}

/* ------------------------- idle-task memory callback ------------------------- *
 * configSUPPORT_STATIC_ALLOCATION = 1 requires the application to supply
 * the idle task's TCB + stack. */

static StaticTask_t g_idle_tcb;
static StackType_t  g_idle_stack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t  **ppxIdleTaskStackBuffer,
                                   uint32_t      *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer   = &g_idle_tcb;
    *ppxIdleTaskStackBuffer = g_idle_stack;
    *pulIdleTaskStackSize   = configMINIMAL_STACK_SIZE;
}

