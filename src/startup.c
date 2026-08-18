#include <stdint.h>
#include "registers.h"
#include "serial_interfaces.h"
#include "FreeRTOS.h"
#include "task.h"

/* Symbols defined by the linker script -- not variables with storage,
 * just addresses computed by the linker. We take their ADDRESS (&),
 * never their value directly. */
extern uint32_t _estack;          /* top of RAM -- initial stack pointer */
extern uint32_t _data_start;      /* .data run address (in RAM) start   */
extern uint32_t _data_end;        /* .data run address (in RAM) end     */
extern uint32_t _data_load_start; /* .data load address (in FLASH)      */
extern uint32_t _bss_start;       /* .bss start (in RAM)                */
extern uint32_t _bss_end;         /* .bss end (in RAM)                  */

extern int main(void);
extern void TIM2_IRQHandler(void);

extern void Default_Handler(void);
extern void Reset_Handler(void);
extern void USART2_IRQHandler(void);
extern void DMA2_Stream0_IRQHandler(void);
extern void I2C1_EV_IRQHandler(void);
extern void I2C1_ER_IRQHandler(void);
extern void EXTI4_IRQHandler(void);
extern void EXTI1_IRQHandler(void);
extern void ADC_IRQHandler(void);
extern void TIM3_IRQHandler(void);

// FreeRTOS's own core-exception handlers, defined in
// FreeRTOS-Kernel/portable/GCC/ARM_CM4F/port.c. These are ARM core
// exceptions (fixed vector positions per the ARMv7-M architecture), not
// peripheral IRQs -- portVECTOR_INDEX_SVC/PENDSV in port.c confirm 11/14;
// SysTick is architecturally fixed at 15.
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);

typedef void (*vector_entry)(void);

/* The vector table itself. Entry 0 is NOT a function pointer that gets
 * called -- it's the raw address value the CPU loads into the stack
 * pointer at power-on, before executing anything. Entry 1 is the real
 * first code that runs. Placed at the very start of FLASH via the
 * ".isr_vector" section, pinned there by the linker script.
 *
 * I2C1_EV (index 47 = IRQ31+16) and I2C1_ER (index 48 = IRQ32+16) are two
 * separate, independent vectors -- AF and every other I2C error condition
 * interrupt via ER, not EV, confirmed against ST's own HAL example
 * (I2Cx_EV_IRQHandler / I2Cx_ER_IRQHandler wired to separate handlers). */
__attribute__((section(".isr_vector")))
vector_entry vector_table[73] = {
    [0] = (vector_entry)&_estack,
    [1] = Reset_Handler,
    [2 ... 10] = Default_Handler,
    [11] = vPortSVCHandler,     // SVC -- launches the very first task
    [12 ... 13] = Default_Handler,
    [14] = xPortPendSVHandler,  // every subsequent context switch
    [15] = xPortSysTickHandler, // RTOS tick -- drives vTaskDelay/timeouts
    [16 ... 22] = Default_Handler,
    [23] = EXTI1_IRQHandler, // EXTI1_IRQn=7, slot 16+7=23 -- clap sensor
    [24 ... 25] = Default_Handler,
    [26] = EXTI4_IRQHandler,
    [27 ... 33] = Default_Handler,
    [34] = ADC_IRQHandler, // ADC_IRQn=18, slot 16+18=34 -- ADC1's Analog Watchdog (laser tripwire)
    [35 ... 43] = Default_Handler,
    [44] = TIM2_IRQHandler,
    [45] = TIM3_IRQHandler, // TIM3_IRQn=29, slot 16+29=45
    [46] = Default_Handler,
    [47] = I2C1_EV_IRQHandler,
    [48] = I2C1_ER_IRQHandler,
    [49 ... 53] = Default_Handler,
    [54] = USART2_IRQHandler,
    [55 ... 71] = Default_Handler,
    [72] = DMA2_Stream0_IRQHandler
};

void Reset_Handler(void)
{
    /* Copy .data from its load address (FLASH) to its run address (RAM),
     * word by word -- exactly the crt0 job described in comp_arch.md. */
    uint32_t *src = &_data_load_start;
    uint32_t *dst = &_data_start;
    while (dst < &_data_end)
    {
        *dst++ = *src++;
    }

    /* Zero .bss -- no copying needed, pure zero-fill. */
    dst = &_bss_start;
    while (dst < &_bss_end)
    {
        *dst++ = 0;
    }

    main();

    /* main() should never return on bare metal (it's an infinite loop),
     * but if it somehow did, trap here rather than fall into undefined
     * memory -- exactly the "PC jumps to garbage" danger from earlier. */
    while (1)
    {
    }
}

void Default_Handler(void)
{
    while (1)
    {
    }
}

volatile int tick_flag = 0;

void TIM2_IRQHandler(void)
{
    int uif = (TIM2_SR >> 0 & 1);
    if (uif == 1)
    {
        TIM2_SR &= ~(1 << 0);  // Clear the update interrupt flag
        GPIOA_ODR ^= (1 << 8);
        tick_flag = 1;         // Tell main() a tick happened; no UART work here
    }
}

volatile int rx_ready_flag = 0;
volatile char received_char = 0;

void USART2_IRQHandler(void) {
    int rxne = (USART2_SR >> 5) & 1;
    if (rxne) {
        received_char = USART2_DR & 0xFF; // Read the received character
        rx_ready_flag = 1; // Set the flag to indicate data is ready to be read
    }
}

volatile uint8_t dma_sample_buffer[20];
volatile int half_sample = 0;

void DMA2_Stream0_IRQHandler(void) { // 3,4
    int half = (DMA2_LISR >> 4) & 1;
    if (half){
        DMA2_LIFCR |= (1 << 4);
        half_sample++;
    }
    int complete = (DMA2_LISR >> 5) & 1;
    if (complete) {
        half_sample++;
        DMA2_LIFCR |= (1 << 5);
    }
}

volatile i2c_bus_state_t i2c_state = I2C_STATE_IDLE;
volatile uint8_t i2c_received_data;
volatile uint8_t i2c_sended_data;
volatile uint32_t current_i2c_address;
volatile int i2c_last_ok;
TaskHandle_t xI2CTaskHandle; // NULL until xTaskCreate() sets it -- zero-initialized
                              // via .bss, same mechanism as Reset_Handler's own clear
TaskHandle_t xFlameTaskHandle; // same zero-init story, target of EXTI4_IRQHandler's notify
TaskHandle_t xLaserTaskHandle; // same zero-init story, target of ADC_IRQHandler's notify
TaskHandle_t xPWMTaskHandle; // same zero-init story, target of vLaserTripTask's xTaskNotify --
                              // note this one's notified from task context, not an ISR
TaskHandle_t xClapTaskHandle; // same zero-init story, target of EXTI1_IRQHandler's notify
TaskHandle_t xIRTaskHandle; // same zero-init story, target of vIRreceiveTask's notify
// EXTI4 -- flame sensor DO, falling edge (module drives DO low once IR
// intensity crosses the onboard comparator's trip point; idles high). ISR
// does the minimum: clear the pending flag, then hand off to task context.
// No debounce here on purpose -- that's vFlameAlarmTask's job, done at task
// priority where a vTaskDelay is cheap; blocking or spin-waiting inside an
// ISR is not.
void EXTI4_IRQHandler(void)
{
    if ((EXTI_PR >> 4) & 1)
    {
        EXTI_PR |= (1 << 4); // clear pending -- write-1-to-clear, same convention as DMA2_LIFCR

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(xFlameTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// EXTI1 -- clap sensor DO, same shape as EXTI4_IRQHandler.
void EXTI1_IRQHandler(void)
{
    if ((EXTI_PR >> 1) & 1)
    {
        EXTI_PR |= (1 << 1);

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(xClapTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// TIM3 CH3 input capture -- IR receiver, PB0. Computes the delta against
// the last capture right here and passes it as the notification value.
void TIM3_IRQHandler(void)
{
    if ((TIM3_SR >> 3) & 1)
    {
        static uint32_t last_capture = 0;
        uint32_t new_capture = TIM3_CCR3; // reading CCR3 clears CC3IF
        uint32_t delta = (uint16_t)(new_capture - last_capture); // truncate to 16 bits --
                                    // ARR is only ~65.5ms now at 1us/tick, so a single wrap
                                    // between captures is possible; this handles it correctly
        last_capture = new_capture;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(xIRTaskHandle, delta, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// ADC1's Analog Watchdog -- laser tripwire. AWD sets whenever a channel-1
// (PA1, photoresistor divider) conversion lands outside HTR/LTR (see the
// watchdog setup in test.c). Same shape as EXTI4_IRQHandler: clear the
// flag, then hand off to task context immediately -- no debounce or
// buzzer logic here, that's vLaserTripTask's job.
void ADC_IRQHandler(void)
{
    if ((ADC1_SR >> 0) & 1) // AWD flag
    {
        ADC1_SR &= ~(1 << 0); // clear -- direct write-0, same convention as
                               // TIM2_SR's UIF, NOT EXTI_PR's write-1-to-clear

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(xLaserTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

// AF (address/data NACKed) -- and every other I2C error condition -- signals
// via the SEPARATE ER interrupt line, not EV. A NACK on address 0x08 (which
// nothing acks) was silently going nowhere: I2C1_EV_IRQHandler correctly
// served SB once, then had no way to ever be re-entered for the NACK that
// followed, since that event was requesting IRQ32 (I2C1_ER), which this
// build never enabled or implemented -- explains "ISR entered exactly once,
// then dead silence" precisely.
void I2C1_ER_IRQHandler(void) {
    if ((I2C1_SR1 >> 10) & 1) {
        I2C1_SR1 &= ~(1 << 10); // clear AF -- direct write, no SR2 read
        I2C1_CR1 |= (1 << 9);   // Generate STOP
        i2c_last_ok = 0;
        i2c_state = I2C_STATE_DONE;

        // Wake vI2CTask -- it's been Blocked in ulTaskNotifyTake() since
        // i2c1_start_transaction() started this transfer. Still have to
        // wake it even on failure, or it stays blocked forever on this
        // address instead of moving on to the next.
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(xI2CTaskHandle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void I2C1_EV_IRQHandler(void) {
    switch (i2c_state) {
        case I2C_STATE_GENERATE_START:
            if ((I2C1_SR1 >> 0) & 1) {
                i2c_state = I2C_STATE_ADDR_WRITE_ACK;
                I2C1_DR = (uint8_t)((current_i2c_address << 1) | 0); // address byte, write direction
            }
            break;
        case I2C_STATE_ADDR_WRITE_ACK:
            if ((I2C1_SR1 >> 1) & 1) {
                i2c_state = I2C_STATE_WRITE_COMPLETE;
                (void)I2C1_SR2; // completes ADDR's clear sequence
                (void)I2C2_SR1; // I2C2 (slave) sets its own ADDR on this same
                (void)I2C2_SR2; // match -- clear it on the slave's behalf too,
                                // same reason as the polling scanner does.
                I2C1_DR = 'A';
                // Known simplification, same as the polling version: this is
                // a self-loopback rig, so the "slave" side has no ISR of its
                // own -- servicing it synchronously here, inside the
                // master's ISR, is a deliberate bring-up shortcut, not
                // something a real two-chip design would do.
                while (((I2C2_SR1 >> 6) & 1) == 0) {}
                i2c_sended_data = I2C2_DR & 0xFF;
            }
            break;
        case I2C_STATE_WRITE_COMPLETE:
            // Waiting for BTF. React by generating the repeated START, and
            // stop there -- DR only gets written once the *next* SB confirms
            // the START actually happened; that's REPEATED_START's job.
            if (((I2C1_SR1 >> 2) & 1) == 1) {
                I2C1_CR1 |= (1 << 8); // repeated start
                i2c_state = I2C_STATE_REPEATED_START;
            }
            break;
        case I2C_STATE_REPEATED_START:
            if ((I2C1_SR1 >> 0) & 1) {
                I2C1_DR = (uint8_t)((current_i2c_address << 1) | 1); // address byte, read direction
                i2c_state = I2C_STATE_ADDR_READ_ACK;
            }
            break;
        case I2C_STATE_ADDR_READ_ACK:
            // Single-byte-read quirk: clear ACK before clearing ADDR, then
            // queue STOP right away -- not after the byte arrives.
            if ((I2C1_SR1 >> 1) & 1) {
                I2C1_CR1 &= ~(1 << 10); // clear ACK
                (void)I2C1_SR2;         // clear ADDR
                (void)I2C2_SR1;
                (void)I2C2_SR2;
                I2C1_CR1 |= (1 << 9); // Generate STOP
                I2C2_DR = 'E';
                i2c_state = I2C_STATE_READ_AVAILABLE;
            }
            break;
        case I2C_STATE_READ_AVAILABLE:
            if (((I2C1_SR1 >> 6) & 1) == 1) {
                i2c_received_data = I2C1_DR & 0xFF;
                i2c_last_ok = 1;
                i2c_state = I2C_STATE_DONE;

                // Wake vI2CTask -- same notify+yield as I2C1_ER's failure
                // path, this is the success path's equivalent.
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                vTaskNotifyGiveFromISR(xI2CTaskHandle, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
            break;
        case I2C_STATE_IDLE:
        case I2C_STATE_DONE:
        default:
            // No transaction in flight -- a firing here is spurious.
            // Explicit no-op cases silence -Wswitch and document the intent.
            break;
    }
}
