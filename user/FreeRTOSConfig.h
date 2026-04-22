/**
 * FreeRTOSConfig.h — tight static-only config for N32WB031 TOTP.
 *
 * Design goals (see C:\Users\SU\.claude\plans\freertos-flash-ram-flash-ram-cached-pike.md):
 *   - minimum flash and RAM footprint
 *   - static allocation only; no heap_x.c, no pvPortMalloc
 *   - three tasks (ble / gui / totp) + the built-in idle task
 *   - Cortex-M0 port, SysTick @ 1 kHz
 *
 * Most optional features are turned OFF. If you later need timers,
 * recursive mutexes, queue sets, etc., turn them on here — each adds
 * flash. Check beacon.map after rebuilding.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ----------------------- scheduler basics ----------------------- */

#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0  /* Cortex-M0 has no CLZ */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ((unsigned long)64000000)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    4     /* idle + 3 user */
#define configMINIMAL_STACK_SIZE                ((uint16_t)96)  /* words */
#define configMAX_TASK_NAME_LEN                 8
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configQUEUE_REGISTRY_SIZE               0
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0

/* ----------------------- memory model ----------------------- */

#define configSUPPORT_STATIC_ALLOCATION         1
#define configSUPPORT_DYNAMIC_ALLOCATION        0

/* ----------------------- strip optional features ----------------------- */

#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configUSE_TASK_NOTIFICATIONS            1   /* RTC→totp_task signal */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   1
#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0
#define configRECORD_STACK_HIGH_ADDRESS         0

/* ----------------------- safety ----------------------- */
/*
 * Keep these off for release — each costs flash. Flip to 2 plus set
 * configUSE_STATS_FORMATTING_FUNCTIONS = 1 during bring-up if you need
 * uxTaskGetStackHighWaterMark / stack-overflow hooks.
 */
#define configCHECK_FOR_STACK_OVERFLOW          0
#define configUSE_MALLOC_FAILED_HOOK            0

/* Disable configASSERT in release; hitting one traps to the debugger. */
#ifdef DEBUG
    #define configASSERT(x) if ((x) == 0) { taskDISABLE_INTERRUPTS(); for (;;); }
#else
    #define configASSERT(x) ((void)0)
#endif

/* ----------------------- API surface ----------------------- */
/* Enable only what the app actually calls. Each 1 costs ~50–200 B flash. */
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    1   /* portMAX_DELAY relies on this */
#define INCLUDE_vTaskDelayUntil                 0
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          0
#define INCLUDE_xTaskGetCurrentTaskHandle       0
#define INCLUDE_uxTaskGetStackHighWaterMark     0
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   0
#define INCLUDE_xTimerPendFunctionCall          0
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xQueueGetMutexHolder            0
#define INCLUDE_xSemaphoreGetMutexHolder        0
#define INCLUDE_xTaskResumeFromISR              0

/* ----------------------- Cortex-M0 port handler names ----------------------- *
 * Let the RVDS/ARM_CM0 port install its handlers by name. The stubs in
 * user/n32wb03x_it.c remap SVC/PendSV/SysTick to these. */
#define vPortSVCHandler     SVC_Handler
#define xPortPendSVHandler  PendSV_Handler
#define xPortSysTickHandler SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
