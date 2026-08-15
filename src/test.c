#include <stdint.h>
#include "registers.h"
#include "serial_interfaces.h"
#include "irq.h"
#include "gpio.h"
#include "FreeRTOS.h"
#include "task.h"
#include "app_tasks.h"

int main(void)
{
    RCC_AHB1ENR |= (1 << 0); // GPIOAEN
    RCC_AHB1ENR |= (1 << 1); // GPIOBEN -- PB3/PB8/PB9/PB10 all need this
    RCC_APB2ENR |= (1 << 14); // SYSCFGEN -- EXTI4's port mux (EXTICR2) needs this

    configure_gpio();

    SYSCFG_EXTICR2 &= ~(0xF << 0); // clear EXTI4's nibble (bits[3:0]) -- was clearing
                                   // bits[4:7], EXTI5's field, by mistake
    SYSCFG_EXTICR2 |= (1 << 0);   // select port B (0001) for EXTI4
    EXTI_IMR |= (1 << 4); // unmask line 4
    EXTI_RTSR |= (1 << 4); // rising edge trigger for line 4 -- confirmed via multimeter
                           // directly on the sensor's DO pin: idles low, drives to 3.3V
                           // on detection. Opposite of the falling-edge assumption this
                           // was originally configured for.

    // NVIC_ISER0 |= (1 << 28); // TIM2
    NVIC_ISER0 |= (1 << 10); // EXTI4
    NVIC_ISER0 |= (1 << 18); // ADC -- Analog Watchdog (laser tripwire)
    NVIC_ISER0 |= (1 << 31); // I2C1_EV
    NVIC_ISER1 |= (1 << 0);  // I2C1_ER -- separate line, IRQ32 (first bit of ISER1's range)
    NVIC_ISER1 |= (1 << 6);  // USART2
    NVIC_ISER1 |= (1 << 24); // DMA2 Stream0

    // EXTI4, I2C1_EV, and I2C1_ER all call FreeRTOS "FromISR" functions now,
    // so all three need their NVIC priority set at/below configMAX_SYSCALL_
    // INTERRUPT_PRIORITY (5) before that's safe -- otherwise they stay at
    // their power-on-reset priority of 0, able to preempt the kernel's own
    // critical sections and corrupt scheduler state. Priority 5 shifted
    // left 4 within its byte (only the top 4 bits are implemented silicon),
    // then placed at the right byte within each word -- IPR2 byte 2 for
    // EXTI4 (IRQ10), IPR7 byte 3 for I2C1_EV (IRQ31), IPR8 byte 0 for
    // I2C1_ER (IRQ32).
    NVIC_IPR2 = (NVIC_IPR2 & ~(0xFFUL << 16)) | (5UL << 20);
    NVIC_IPR7 = (NVIC_IPR7 & ~(0xFFUL << 24)) | (5UL << 28);
    NVIC_IPR8 = (NVIC_IPR8 & ~(0xFFUL << 0)) | (5UL << 4);
    // ADC's ISR doesn't call any FreeRTOS "FromISR" function yet -- this
    // isn't strictly required until it does -- but setting it now avoids
    // the exact "forgot the priority, stuck at 0" bug already caught once
    // on EXTI4 this project.
    NVIC_IPR4 = (NVIC_IPR4 & ~(0xFFUL << 16)) | (5UL << 20);

    RCC_APB1ENR |= (1 << 0); // TIM2 clock
    TIM2_PSC = 15;
    TIM2_ARR = 0xFFFFFFFF;
    // TIM2_DIER |= (1 << 0);
    TIM2_CR1 |= (1 << 0);

    RCC_APB1ENR |= (1 << 2); // Enable TIM4 clock
    // PSC bumped from 1599 -- that gave ~99Hz, fine for the LED's breathing
    // fade but inaudible/unusable as a buzzer tone on CH2, which shares this
    // same PSC/ARR (base frequency). ~157 keeps ARR=100 unchanged and lands
    // both channels around ~1kHz -- audible on the buzzer, imperceptibly
    // different on the LED fade.
    TIM4_PSC = 157;
    TIM4_ARR = 100;
    TIM4_CCMR1 |= (1 << 3); // Enable preload for channel 1
    TIM4_CCMR1 &= ~(0x7 << 4);
    TIM4_CCMR1 |= (0x6 << 4); // Set channel 1 to PWM mode 1
    TIM4_CCER |= (1 << 0); // Enable channel 1 output
    TIM4_CCR1 = 50; // Set duty cycle to 50%

    // CH2 -- passive buzzer, PB7. Same PWM mode 1 setup as CH1, mirrored at
    // CH2's own bit positions in CCMR1/CCER. CC2E is left OFF here --
    // vPWMTimerTask gates it on/off itself to produce the pulsed alarm
    // pattern, rather than driving it continuously like CH1's LED.
    TIM4_CCMR1 |= (1 << 11); // Enable preload for channel 2
    TIM4_CCMR1 &= ~(0x7 << 12);
    TIM4_CCMR1 |= (0x6 << 12); // Set channel 2 to PWM mode 1
    TIM4_CCR2 = 50; // 50% duty -- square wave, not used as a fade level like CCR1

    TIM4_CR1 |= (1 << 0);

    RCC_APB1ENR |= (1 << 17);                      // Enable USART2 clock
    USART2_BRR = (8 << 4) | 11;                    // Set baud rate to 115200 (assuming 16 MHz clock)
    USART2_CR1 |= (1 << 3) | (1 << 2) | (1 << 13); // Enable transmitter, receiver, and USART
    USART2_CR1 |= (1 << 5); // Enable RXNE interrupt

    RCC_APB2ENR |= (1 << 12);
    SPI1_CR1 |= (1 << 2) | (1 << 8) | (1 << 9); // ENABLE SPI, SSM, SSI
    SPI1_CR1 &= ~(0x7 << 3);
    SPI1_CR1 |= (0x5 << 3); // set baud rate prescaler to 64
    SPI1_CR1 |= (1 << 6); //enable SPI1

    RCC_APB2ENR |= (1 << 8); // Enable ADC1 clock
    ADC1_CR1 &= ~(0x3 << 24);
    ADC1_CR1 |= (0x2 << 24); // Set resolution to 8 bits

    // Analog Watchdog -- laser tripwire. Watches channel 1 (PA1, the
    // photoresistor divider) and lets ADC1 itself catch a beam-break by
    // comparing every conversion against HTR/LTR in hardware -- no
    // software polling needed, unlike the flame sensor's task-side check.
    // Measured swing on this divider: ~9 (laser hitting the sensor
    // dead-on) vs ~208 (beam physically blocked).
    ADC1_CR1 &= ~(0x1F << 0);
    ADC1_CR1 |= (1 << 0);  // AWDCH -- watch channel 1 (PA1)
    ADC1_CR1 |= (1 << 9);  // AWDSGL -- restrict the watchdog to that one channel
    ADC1_CR1 |= (1 << 6);  // AWDIE -- let AWD actually request an interrupt
    ADC1_CR1 |= (1 << 23); // AWDEN -- enable the watchdog on regular channels

    // HTR/LTR compared directly against the same right-aligned value that
    // ends up in DR/dma_sample_buffer[0] (ALIGN=0 below) -- confirmed
    // against RM0368 directly, no resolution-based bit shift documented
    // for these registers. Earlier attempt shifted this left by 4
    // (assuming a 12-bit-vs-8-bit MSB alignment that doesn't actually
    // exist for HTR/LTR on this chip) and the watchdog never tripped --
    // corrected to a plain 0-255-scale value.
    ADC1_HTR = 100; // trip point ~100 on the 8-bit scale -- well above the
                     // ~9 resting baseline, well below the ~208 blocked reading
    ADC1_LTR = 0; // no lower bound needed for this application

    ADC1_SMPR2 &= ~(0x7 << 3);
    ADC1_SMPR2 |= (0x7 << 3);

    ADC1_CR2 |= (1 << 0); // Enable ADC
    ADC1_CR2 &= ~(1 << 11); // Enable right alignment
    ADC1_CR2 |= (1 << 1); // Enable continious mode
    ADC1_CR2 |= (1 << 8); //Enable DMA
    ADC1_CR2 |= (1 << 9); //Enable DDS

    ADC1_SQR1 &= ~(0xF << 20); //set number of conversions to 1
    ADC1_SQR3 &= ~(0x1F << 0); 
    ADC1_SQR3 |= (1 << 0); // SQ1 now points at channel 1 (PA1) instead of channel 0

    RCC_AHB1ENR |= (1 << 22);
    DMA2_S0CR |= (1 << 8) | (1 << 10) | (1 << 3) | (1 << 4);
    DMA2_S0CR &= ~(0x3 << 11); //set data size to 8 bit
    DMA2_S0CR &= ~(0x3 << 13);  //set memory size to 8 bit
    DMA2_S0CR &= ~(0x3 << 6); //set direction = peripheral to memory
    DMA2_S0CR &= ~(0x7 << 25); //select channel 0
    DMA2_S0FCR &= ~(1 << 2); // set the direct mode explicitly
    DMA2_S0PAR = (uint32_t)&ADC1_DR;
    DMA2_S0M0AR = (uint32_t)dma_sample_buffer;
    DMA2_S0NDTR = sizeof(dma_sample_buffer) / sizeof(dma_sample_buffer[0]);

    DMA2_S0CR |= (1 << 0); //set EN to 1
    ADC1_CR2 |= (1 << 30); // start converison

    // I2C1 -- master, same historically-verified FREQ/CCR/TRISE, PE
    // enabled last, plus interrupt enables now that the ISR is doing the
    // clocking instead of a polling loop. ITEVFEN covers SB/ADDR/BTF/STOPF;
    // ITBUFEN additionally covers TxE/RxNE; ITERREN covers AF.
    RCC_APB1ENR |= (1 << 21);
    I2C1_CR2 &= ~(0x3F << 0);
    I2C1_CR2 |= (16 << 0);
    I2C1_CR2 |= (1 << 8);  // ITERREN -- AF needs this to actually interrupt
    I2C1_CR2 |= (1 << 9);  // ITEVFEN
    I2C1_CR2 |= (1 << 10); // ITBUFEN
    I2C1_CCR &= ~(1 << 15); //set I2C to standart mode
    I2C1_CCR &= ~(0xFFF << 0);
    I2C1_CCR |= (80 << 0); //calculate and set I2C CCR value
    I2C1_TRISE &= ~(0x3F << 0);
    I2C1_TRISE |= (17 << 0); // calculate and set TRISE
    I2C1_CR1 |= (1 << 0);

    // I2C2 -- slave, loopback target for I2C1's scanner. Same PCLK1
    // (16 MHz), so FREQ/CCR/TRISE mirror I2C1's values.
    RCC_APB1ENR |= (1 << 22); // Enable I2C2 clock
    I2C2_CR2 &= ~(0x3F << 0);
    I2C2_CR2 |= (16 << 0);
    I2C2_CCR &= ~(1 << 15);
    I2C2_CCR &= ~(0xFFF << 0);
    I2C2_CCR |= (80 << 0);
    I2C2_TRISE &= ~(0x3F << 0);
    I2C2_TRISE |= (17 << 0);

    // OAR1: bit 14 mandatory always-1; 7-bit address 0x30 at bits [7:1].
    I2C2_OAR1 |= (1 << 14);
    I2C2_OAR1 &= ~(0x7F << 1);
    I2C2_OAR1 |= (0x30 << 1);

    // ACK is gated by PE (cleared by hardware while PE=0) -- PE must go
    // high first, or ACK gets silently zeroed back out.
    I2C2_CR1 |= (1 << 0);  // PE first
    I2C2_CR1 |= (1 << 10); // ACK -- now it actually latches

    //xTaskCreate(&vThermistorTask, "Thermo", 128, NULL, 1, NULL);
    // xTaskCreate(&vOneWireTask, "OneWire", 512, NULL, 1, NULL);
    // Priority 2, one above Thermo -- an alarm response should get the CPU
    // ahead of routine periodic printing once both are ready to run. Handle
    // captured into xFlameTaskHandle so EXTI4_IRQHandler has a target to
    // notify.
    xTaskCreate(&vFlameAlarmTask, "Flame", 128, NULL, 2, &xFlameTaskHandle);
    xTaskCreate(&vLaserTripTask, "Laser", 128, NULL, 2, &xLaserTaskHandle);
    // vPWMTimerTask existed since the LED milestone but was never actually
    // created here -- it wasn't running under the scheduler at all until now.
    xTaskCreate(&vPWMTimerTask, "PWM", 128, NULL, 1, &xPWMTaskHandle);
    vTaskStartScheduler(); // never returns -- the scheduler takes over the CPU
    

    return 0;
}
