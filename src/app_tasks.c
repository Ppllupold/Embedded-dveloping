#include "serial_interfaces.h"
#include "onewire.h"
#include "registers.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_tasks.h"


void vI2CTask(void *pvParameters)
{
    (void)pvParameters; // unused -- this task needs no per-instance data,
                         // same conclusion you reached back when we designed
                         // xTaskCreate's argument list

    for (;;) // tasks never return -- same rule as always
    {
        for (uint8_t address = 0x08; address <= 0x77; address++)
        {
            if (i2c1_start_transaction(address))
            {
                // Running -> Blocked here. Zero CPU spent waiting -- the
                // scheduler runs the PWM task (or the idle task) during
                // this gap. Wakes when either I2C1_EV_IRQHandler's
                // READ_AVAILABLE case (success) or I2C1_ER_IRQHandler's AF
                // case (failure) signals it -- both notify, so this always
                // unblocks one way or the other, never forever.
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

                // Task context now, not interrupt context -- safe to do the
                // blocking UART TXE waits inside usart2_write_byte() that
                // would've been forbidden inside either ISR. Only print on
                // a real ACK'd transfer -- i2c_last_ok distinguishes that
                // from an AF-aborted one, so non-ACKing addresses don't
                // print stale/meaningless data.
                if (i2c_last_ok)
                {
                    usart2_write_uint16(i2c_sended_data);
                    usart2_write_uint16(i2c_received_data);
                    usart2_write_byte('\r');
                    usart2_write_byte('\n');
                }
            }
            // i2c1_start_transaction() returning 0 means the bus was busy --
            // skip this address for this pass rather than block forever.
        }

        vTaskDelay(pdMS_TO_TICKS(1000)); // pace between full sweeps
    }
}

// -----------------------------------------------------------------------
// FreeRTOS application hooks -- not optional despite the name. Both are
// unconditionally called by the kernel now that configCHECK_FOR_STACK_OVERFLOW
// and configUSE_MALLOC_FAILED_HOOK are set in FreeRTOSConfig.h. Without these
// two functions existing somewhere, the real ARM build compiles fine but
// fails to LINK -- "undefined reference," not a syntax error, which is why
// host-gcc -fsyntax-only never caught it.
// -----------------------------------------------------------------------


void vOneWireTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        ds18b20_read_temperature();
        vTaskDelay(pdMS_TO_TICKS(2000)); // pace between readings
    }
}

void vThermistorTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        // dma_sample_buffer[0] instead of adc1_read() -- DMA is already
        //  this from ADCcontinuously refilling1 in the background, so
        // this is just peeking at the latest sample. A single-byte read
        // needs no HTIF/TCIF synchronization (see DMA milestone notes) --
        // that discipline only protects multi-sample batch reads from
        // being torn mid-copy.
        usart2_write_uint16(dma_sample_buffer[0]);
        usart2_write_byte('\r');
        usart2_write_byte('\n');
        vTaskDelay(pdMS_TO_TICKS(2000)); // pace between readings
    }
}

void vPWMTimerTask(void *pvParameters)
{
    (void)pvParameters; // unused -- this task needs no per-instance data,
                         // same conclusion you reached back when we designed
                         // xTaskCreate's argument list

    static int alarm_ticks_remaining = 0; // counts down in units of this
                                           // task's own 10ms cadence -- 500
                                           // of them is 5 seconds, the
                                           // siren's total duration
    static int siren_ticks_cycle = 0;     // counts down within one up/down
                                           // sweep cycle (100 ticks = 1
                                           // second), reset to 100 every
                                           // time it hits 0 -- drives which
                                           // half of the cycle (rising vs
                                           // falling) this pass is in
    static float psc_value = 263.0f;      // fractional PSC position. TIM4_PSC
                                           // itself can only hold a whole
                                           // number, but the true per-tick
                                           // step (132 counts / 50 ticks =
                                           // 2.64) isn't one -- tracking it
                                           // here instead avoids the register
                                           // truncating 2.64 down to 2 every
                                           // single tick and falling short of
                                           // the intended 1200Hz peak

    for (;;) // tasks never return -- same rule as always
    {
        // Non-blocking check for a fresh trigger from vLaserTripTask.
        // Zero timeout -- this never blocks, so it can't stall anything else
        // in this loop even when no siren is pending. eSetValueWithOverwrite
        // on the sending side means only the latest trigger matters; we
        // don't care about queuing multiple.
        uint32_t notify_value = 0;
        if (xTaskNotifyWait(0, 0xFFFFFFFF, &notify_value, 0) == pdTRUE)
        {
            psc_value = 263.0f; // 600Hz -- the low end of the sweep, and
                                 // where every cycle starts and ends
            TIM4_PSC = 263;
            alarm_ticks_remaining = 500; // 500 * 10ms = 5 seconds
            siren_ticks_cycle = 100;     // 100 ticks = 1 second per cycle

            TIM4_CCER |= (1 << 4); // CC2E on -- stays on continuously for
                                    // the whole siren. Unlike the old pulsed
                                    // alarm, nothing gates this off and on;
                                    // the sweeping frequency itself is the
                                    // effect this time.
        }

        if (alarm_ticks_remaining > 0)
        {
            if (siren_ticks_cycle > 50)
            {
                psc_value -= 2.64f; // rising half -- PSC falling, pitch climbing
            }
            else
            {
                psc_value += 2.64f; // falling half -- PSC climbing back up,
                                     // pitch dropping back toward 600Hz
            }
            TIM4_PSC = (uint32_t)psc_value; // register only ever sees the
                                             // truncated whole-number part

            siren_ticks_cycle--;
            alarm_ticks_remaining--;

            if (siren_ticks_cycle == 0)
            {
                siren_ticks_cycle = 100; // one cycle finished -- start the next
            }

            if (alarm_ticks_remaining == 0)
            {
                TIM4_CCER &= ~(1 << 4); // CC2E off -- force silent at the
                                         // end, same "don't leave it stuck
                                         // mid-tone" reasoning as the old
                                         // pulsed alarm's final clear
            }
        }

        // LED breathing fade -- unchanged, keeps running every pass
        // regardless of whether an alarm is active.
        // static int ccr_value = 50;
        // static int direction = 1; // 1 for increasing, -1 for decreasing

        // if (ccr_value == 100)
        // {
        //     direction = -1; // Change direction to decreasing
        // }
        // else if (ccr_value == 0)
        // {
        //     direction = 1; // Change direction to increasing
        // }
        // TIM4_CCR1 = ccr_value; // Update the CCR1 value
        // ccr_value += direction;
         vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void vFlameAlarmTask(void *pvParameters)
{
    (void)pvParameters; // unused -- same conclusion as every other task here

    for (;;) // tasks never return -- same rule as always
    {
        // Running -> Blocked here, zero CPU cost, until EXTI4_IRQHandler
        // notifies this task on a falling edge from the sensor's DO
        // line (idles high, drives low on detection -- confirmed by
        // multimeter). Same wait shape as vI2CTask's ulTaskNotifyTake --
        // the interrupt is the only thing that gets us past this line.
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // TEMPORARY -- tap module test: it's a momentary switch, not a
        // level-persistent comparator, so the usual debounce-then-recheck
        // (below, disabled for now) finds the pin already back at idle and
        // swallows every real tap. Just beep straight off the notify instead.
        GPIOA_BSRR = (1 << 4); // buzzer on -- PA4, BS[4]
        vTaskDelay(pdMS_TO_TICKS(200));
        GPIOA_BSRR = (1 << (16 + 4)); // buzzer off -- PA4, BR[4]

        // Debounce, from task context, not the ISR: a comparator sitting
        // right at its trip point can chatter for a few microseconds before
        // settling, generating a spurious edge. Re-sampling the pin after a
        // real delay filters that without slowing down the interrupt path
        // itself -- deferring real work out of the ISR, same discipline
        // this project has used everywhere else.
        // vTaskDelay(pdMS_TO_TICKS(20));
        //
        // if (((GPIOB_IDR >> 4) & 1) == 1) // still high -- detection condition really holds
        // {
        //     GPIOA_BSRR = (1 << 4); // buzzer on -- PA4, BS[4]
        //
        //     usart2_write_byte('F');
        //     usart2_write_byte('!');
        //     usart2_write_uint16(dma_sample_buffer[0]); // crude, but enough to see if the ADC is
        //     usart2_write_byte('\r');
        //     usart2_write_byte('\n');
        //
        //     // Hold the alarm until the sensor itself reports clear.
        //     while (((GPIOB_IDR >> 4) & 1) == 1)
        //     {
        //         vTaskDelay(pdMS_TO_TICKS(200));
        //     }
        //
        //     GPIOA_BSRR = (1 << (16 + 4)); // buzzer off -- PA4, BR[4]
        // }
        // else: the debounce re-check found the line already back low --
        // treat it as chatter, not a real detection, and loop back to
        // blocking rather than falsely alarming.
    }
}

void vLaserTripTask(void *pvParameters)
{
    (void)pvParameters; 

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        vTaskDelay(pdMS_TO_TICKS(20));

        if (dma_sample_buffer[0] > 100) // still above the trip point -- beam still broken
        {
            GPIOA_BSRR = (1 << 4); // buzzer on -- PA4, BS[4]
            xTaskNotify(xPWMTaskHandle, 1, eSetValueWithOverwrite); // Wake vPWMTimerTask for the 3-second passive-buzzer alarm tone

            while (dma_sample_buffer[0] > 100) // Hold the alarm until the reading itself drops below the trip point 

            {
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            GPIOA_BSRR = (1 << (16 + 4)); // buzzer off -- PA4, BR[4]
        }
    }
}

#define TV_POWER_CODE 111105792

static void ir_transmit(uint32_t code)
{
    TIM4_CCER |= (1 << 0);
    delay_us(9000);
    TIM4_CCER &= ~(1 << 0);
    delay_us(4500);

    for (int i = 0; i < 32; i++)
    {
        TIM4_CCER |= (1 << 0);
        delay_us(562);
        TIM4_CCER &= ~(1 << 0);
        delay_us(((code >> i) & 1) ? 1687 : 562);
    }

    usart2_write_byte('I');
    TIM4_CCER |= (1 << 0);
    delay_us(562);
    TIM4_CCER &= ~(1 << 0);
}

void vClapTask(void *pvParameters)
{
    (void)pvParameters;

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (((GPIOB_IDR >> 1) & 1) == 0) // still low -- real trigger (idles high)
        {
            usart2_write_byte('C');
            usart2_write_byte('!');
            usart2_write_byte('\r');
            usart2_write_byte('\n');

            ir_transmit(TV_POWER_CODE);
        }
    }
}

void vIRreceiveTask(void *pvParameters)
{
    (void)pvParameters;

    uint32_t accumulator = 0;
    uint8_t counter = 0;

    for (;;)
    {
        uint32_t delta;
        xTaskNotifyWait(0, 0, &delta, portMAX_DELAY);

        if (delta > 1700) { // midpoint between a real 0-bit (~1125us) and 1-bit (~2250us)
            if (delta > 5000) { // header -- reset for a new frame
                accumulator = 0;
                counter = 0;
            } else { // data bit 1
                accumulator |= (1UL << counter);
                counter++;
            }
        } else { // data bit 0 -- accumulator already has 0 there, just advance
            counter++;
        }

        if (counter == 32) {
            usart2_write_uint32(accumulator);
            usart2_write_byte('\r');
            usart2_write_byte('\n');
            counter = 0; 
        }
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask; // the handle itself isn't printable -- the name is what's useful

    // Best-effort diagnostic: the offending task's own stack is already
    // corrupted, so this print is a calculated risk, not a guarantee -- but
    // a silent hang tells you nothing, and this project has leaned on UART
    // diagnostics everywhere else. Worth the attempt.
    while (*pcTaskName != '\0')
    {
        usart2_write_byte(*pcTaskName);
        pcTaskName++;
    }
    usart2_write_byte('\r');
    usart2_write_byte('\n');

    // Never continue past this -- stack corruption isn't a recoverable
    // state. Disable interrupts first so the scheduler can't try to context
    // switch on top of whatever's left of this task's stack.
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void vApplicationMallocFailedHook(void)
{
    // No task/size context available here, unlike the stack-overflow hook --
    // pvPortMalloc() doesn't pass one. A fixed message is the best this can do.
    const char *msg = "MALLOC FAILED\r\n";
    while (*msg != '\0')
    {
        usart2_write_byte(*msg);
        msg++;
    }

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}