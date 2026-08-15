#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* -----------------------------------------------------------------------
 * Project-owned FreeRTOS config for the Nucleo-F401RE.
 *
 * Clock: this project never touches RCC's PLL config, so the chip is still
 * running on its reset-default HSI oscillator, undivided -- SYSCLK = HCLK =
 * PCLK1 = PCLK2 = 16 MHz. Confirmed against this project's own I2C1_CR2
 * FREQ field (set to 16, matching a 16 MHz PCLK1). If a PLL setup is ever
 * added later, this value MUST be updated to match, or every vTaskDelay()
 * and timeout in the system silently drifts.
 * ------------------------------------------------------------------- */
#define configCPU_CLOCK_HZ                       ( ( unsigned long ) 16000000 )

/* Tick rate: resolution-vs-overhead trade-off already covered -- 1000 Hz
 * gives 1 ms delay granularity. Independent of this project's own TIM2
 * tick_flag timer; SysTick is separate hardware. */
#define configTICK_RATE_HZ                       ( ( TickType_t ) 1000 )

#define configUSE_PREEMPTION                     1
#define configUSE_TIME_SLICING                   1
#define configUSE_TICKLESS_IDLE                  0
#define configUSE_PORT_OPTIMISED_TASK_SELECTION  0

#define configMAX_PRIORITIES                     5
#define configMINIMAL_STACK_SIZE                 ( ( unsigned short ) 128 )  /* words, not bytes */
#define configMAX_TASK_NAME_LEN                  16
#define configUSE_16_BIT_TICKS                   0  /* 32-bit tick count -- avoids early rollover */
#define configIDLE_SHOULD_YIELD                  1

#define configUSE_MUTEXES                        1
#define configUSE_COUNTING_SEMAPHORES            1
#define configUSE_TASK_NOTIFICATIONS             1  /* needed for vTaskNotifyGiveFromISR */
#define configTASK_NOTIFICATION_ARRAY_ENTRIES    1
#define configUSE_TIMERS                         0  /* not using software timers yet */

#define configSUPPORT_STATIC_ALLOCATION          0
#define configSUPPORT_DYNAMIC_ALLOCATION         1
#define configTOTAL_HEAP_SIZE                    ( ( size_t ) ( 8 * 1024 ) )  /* 8 KB of this chip's 96 KB SRAM */

/* Safety nets worth their cost: both catch exactly the "silent corruption"
 * failure modes flagged earlier (stack overflow with no hardware guard,
 * and a failed pvPortMalloc inside xTaskCreate going unnoticed). Both need
 * a hook function defined somewhere in the project -- stubbed in startup.c. */
#define configCHECK_FOR_STACK_OVERFLOW           2
#define configUSE_MALLOC_FAILED_HOOK             1
#define configUSE_IDLE_HOOK                      0
#define configUSE_TICK_HOOK                      0

#define configQUEUE_REGISTRY_SIZE                0
#define configUSE_MINI_LIST_ITEM                 1
#define configSTACK_DEPTH_TYPE                   uint16_t
#define configMESSAGE_BUFFER_LENGTH_TYPE         size_t

/* Optional API inclusions -- functions this project actually calls. */
#define INCLUDE_vTaskDelay                       1
#define INCLUDE_vTaskSuspend                     1
#define INCLUDE_vTaskDelete                      1
#define INCLUDE_uxTaskPriorityGet                1
#define INCLUDE_vTaskPrioritySet                 1
#define INCLUDE_xTaskGetSchedulerState           1
#define INCLUDE_xTaskGetCurrentTaskHandle        1
#define INCLUDE_uxTaskGetStackHighWaterMark      1 // for right-sizing vOneWireTask's stack

/* -----------------------------------------------------------------------
 * Cortex-M interrupt priority setup. STM32F4 implements 4 priority bits
 * (16 levels: 0-15, numerically lower = more urgent -- the NVIC rule from
 * this project's own interrupts milestone).
 *
 * configMAX_SYSCALL_INTERRUPT_PRIORITY is the boundary xPortPendSVHandler
 * masks via BASEPRI during a critical section. ANY ISR that calls a
 * FreeRTOS "FromISR" function -- which I2C1_EV_IRQHandler is about to,
 * once the notification call gets added -- MUST have its own NVIC
 * priority set NUMERICALLY AT OR BELOW this urgency (i.e. >= 5 here), or
 * it can preempt the kernel's own critical sections and corrupt its
 * internal data structures. This is silent and intermittent on real
 * hardware, not a clean crash -- exactly the failure mode this project
 * has been trying to catch before it happens, not after.
 *
 * IMPORTANT, NOT YET DONE: every ISR in this project (TIM2, USART2, DMA2,
 * I2C1) has only ever been enabled via NVIC_ISERx -- none has ever had its
 * priority level explicitly set via the NVIC_IPRx registers, so all of
 * them are still sitting at their power-on-reset default of priority 0 --
 * the HIGHEST possible urgency, well above this boundary. I2C1_EV_IRQn's
 * priority must be explicitly set to >= 5 with NVIC_SetPriority() (or the
 * raw IPR register) before the notification call goes into its ISR. Flag
 * this as required, unfinished work -- not solved yet.
 * ------------------------------------------------------------------- */
#define configPRIO_BITS                          4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY  15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

#define configASSERT( x ) if( ( x ) == 0 ) { taskDISABLE_INTERRUPTS(); for( ;; ); }

#endif /* FREERTOS_CONFIG_H */
