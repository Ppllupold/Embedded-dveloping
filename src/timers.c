#include "registers.h"
#include "timers.h"

void configure_timers(void)
{
    RCC_APB1ENR |= (1 << 0); // TIM2 clock
    TIM2_PSC = 15;
    TIM2_ARR = 0xFFFFFFFF;
    // TIM2_DIER |= (1 << 0);
    TIM2_CR1 |= (1 << 0);


    // TIM3 -- IR receiver, Input Capture on CH3/PB0.
    RCC_APB1ENR |= (1 << 1); // Enable TIM3 clock
    TIM3_CCMR2 &= ~(0x3 << 0); // 
    TIM3_CCMR2 |= (1 << 0);  // direct Capture mode
    TIM3_CCER |= (1 << 9); 
    TIM3_CCER &= ~(1 << 11); 
    TIM3_CCER |= (1 << 8);
    TIM3_PSC = 15; // 1us/tick now, not 1ms -- real margin around NEC's own timing
    TIM3_ARR = 0xFFFF;
    TIM3_DIER |= (1 << 3); // CC3IE
    TIM3_CR1 |= (1 << 0);




    RCC_APB1ENR |= (1 << 2); // Enable TIM4 clock
    // 38kHz IR carrier, PB6/CH1 -- PSC=0, ARR=420 -> 16MHz/421 = ~38.005kHz.
    // CC1E left OFF here -- ir_transmit() in app_tasks.c gates it on/off
    // per mark/space, same pattern CH2 already used for the old buzzer.
    TIM4_PSC = 0;
    TIM4_ARR = 420;
    TIM4_CCMR1 |= (1 << 3); // Enable preload for channel 1
    TIM4_CCMR1 &= ~(0x7 << 4);
    TIM4_CCMR1 |= (0x6 << 4); // Set channel 1 to PWM mode 1
    TIM4_CCR1 = 140; // ~33% duty

    // CH2 -- passive buzzer, PB7. Same PWM mode 1 setup as CH1, mirrored at
    // CH2's own bit positions in CCMR1/CCER. CC2E is left OFF here --
    // vPWMTimerTask gates it on/off itself to produce the pulsed alarm
    // pattern, rather than driving it continuously like CH1's LED.
    TIM4_CCMR1 |= (1 << 11); // Enable preload for channel 2
    TIM4_CCMR1 &= ~(0x7 << 12);
    TIM4_CCMR1 |= (0x6 << 12); // Set channel 2 to PWM mode 1
    TIM4_CCR2 = 50; // 50% duty -- square wave, not used as a fade level like CCR1

    TIM4_CR1 |= (1 << 0);
}
