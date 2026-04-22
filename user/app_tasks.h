#ifndef __APP_TASKS_H__
#define __APP_TASKS_H__

#include "FreeRTOS.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Stack size policy (in StackType_t words — 4 bytes each) ----- */
#define BLE_TASK_STACK_WORDS   2048
#define GUI_TASK_STACK_WORDS   1596
#define TOTP_TASK_STACK_WORDS  96

/* Task bodies. Declared here so main.c can create the tasks explicitly
 * with xTaskCreateStatic(). */
void ble_task(void *arg);
void gui_task(void *arg);
void totp_task(void *arg);

/* Static TCB / stack storage owned by app_tasks.c. Exposed so the
 * xTaskCreateStatic() calls in main.c can see them without the task
 * bodies and their buffers drifting apart. */
extern StaticTask_t g_ble_tcb;
extern StackType_t  g_ble_stack[BLE_TASK_STACK_WORDS];

extern StaticTask_t g_gui_tcb;
extern StackType_t  g_gui_stack[GUI_TASK_STACK_WORDS];

extern StaticTask_t g_totp_tcb;
extern StackType_t  g_totp_stack[TOTP_TASK_STACK_WORDS];

/* Handles populated by main.c after task creation — used by ISRs
 * (e.g. RTC_IRQHandler in user/n32wb03x_it.c) for task notifications. */
extern TaskHandle_t g_ble_task_handle;
extern TaskHandle_t g_gui_task_handle;
extern TaskHandle_t g_totp_task_handle;

#ifdef __cplusplus
}
#endif

#endif /* __APP_TASKS_H__ */
