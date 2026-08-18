#include "registers.h"
#include "gpio.h"

// ---------------------------------------------------------------------
// MODER -- 2 bits per pin. Encoding: 00=input 01=output 10=alt-function
// 11=analog. Pin n's field starts at bit (2*n).
//
// AFRL -- 4 bits per pin, PINS 0-7 ONLY (pins 8-15 need AFRH instead).
// Selects which of AF0-AF15 that pin's alternate-function hardware routes
// to. Pin n's field starts at bit (4*n).
//
// Rule that has bitten this project more than once: always clear the full
// field first with a mask at the SAME shift you're about to write with,
// then OR in the new value at that identical shift. A mismatched shift
// between the clear and the set touches the wrong pin entirely -- for
// MODER specifically, that can mean silently reprogramming PA13/PA14
// (the SWD debug pins), which can sever the debugger connection to the
// board.
// ---------------------------------------------------------------------

void configure_gpio(void)
{   

    //PA0 -- ADC1_IN0, analog mode (11).
    GPIOA_MODER &= ~(0x3 << 0);
    GPIOA_MODER |= (0x3 << 0);

    //PA1 -- ADC1_IN1, analog mode (11).
    GPIOA_MODER &= ~(0x3 << 2);
    GPIOA_MODER |= (0x3 << 2);

    // PA2 -- USART2_TX, alternate function AF7.
    GPIOA_MODER &= ~(0x3 << 4);
    GPIOA_MODER |= (0x2 << 4);
    GPIOA_AFRL &= ~(0xF << 8);
    GPIOA_AFRL |= (0x7 << 8);

    // PA3 -- USART2_RX, alternate function AF7.
    GPIOA_MODER &= ~(0x3 << 6);
    GPIOA_MODER |= (0x2 << 6);
    GPIOA_AFRL &= ~(0xF << 12);
    GPIOA_AFRL |= (0x7 << 12);

    //PA4 output
    GPIOA_MODER &= ~(0x3 << 8);
    GPIOA_MODER |= (1 << 8);

    // PA5 -- SPI1_SCK, alternate function AF5.
    GPIOA_MODER &= ~(0x3 << 10);
    GPIOA_MODER |= (0x2 << 10);
    GPIOA_AFRL &= ~(0xF << 20);
    GPIOA_AFRL |= (0x5 << 20);

    // PA6 -- SPI1_MISO, alternate function AF5.
    GPIOA_MODER &= ~(0x3 << 12);
    GPIOA_MODER |= (0x2 << 12);
    GPIOA_AFRL &= ~(0xF << 24);
    GPIOA_AFRL |= (0x5 << 24);

    // PA7 -- SPI1_MOSI, alternate function AF5.
    GPIOA_MODER &= ~(0x3 << 14);
    GPIOA_MODER |= (0x2 << 14);
    GPIOA_AFRL &= ~(0xF << 28);
    GPIOA_AFRL |= (0x5 << 28);

    // PA8 -- GPIO output for LED.
    GPIOA_MODER &= ~(0x3 << 16);
    GPIOA_MODER |= (1 << 16);



    // PB2 -- active buzzer signal, plain push-pull output (OTYPER left at
    // its reset default of 0 = push-pull, same as PA8's LED). Worth knowing:
    // PB2 doubles as BOOT1, sampled only during the reset boot-mode check,
    // and only when BOOT0=1 -- on this board BOOT0 is tied low (boot from
    // main flash), so BOOT1's value is never actually read, and PB2 is a
    // completely ordinary GPIO for the rest of runtime.
    GPIOB_MODER &= ~(0x3 << 4);
    GPIOB_MODER |= (1 << 4);
    
    // PB4 -- flame sensor DO. Internal pull-down added after finding this
    // line floats to an undefined HIGH reading whenever the sensor module
    // isn't actively driving it (e.g. physically unplugged) -- with no
    // pull configured, that float was read as a permanent "flame
    // detected." The sensor's own comparator output is low-impedance, so
    // it still overrides this weak internal pull-down cleanly whenever
    // it's actually connected and driving the line.
    GPIOB_MODER &= ~(0x3 << 8);
    GPIOB_PUPDR &= ~(0x3 << 8);
    GPIOB_PUPDR |= (0x2 << 8); // pull-down (10)


    // PB5 -- OneWire data, moved off PB0 (ADC1_IN8, not 5V-tolerant) now that
    // the DS18B20 needs 5V to clear its internal ~1.5V drop. PB5 is a plain
    // digital pin, not on the ADC mux, so it's 5V-tolerant (FT).
    GPIOB_ODR |= (1 << 5);
    GPIOB_OTYPER |= (1 << 5); // set PB5 as open-drain
    GPIOB_MODER &= ~(0x3 << 10);
    GPIOB_MODER |= (1 << 10); // set PB5 as output

    //PB8 I2C1 SCL, alternate function AF4, open-drain
    GPIOB_OTYPER |= (1 << 8);
    GPIOB_MODER &= ~(0x3 << 16);
    GPIOB_MODER |= (0x2 << 16);
    GPIOB_AFRH &= ~(0xF << 0);
    GPIOB_AFRH |= (0x4 << 0);

    //PB9 I2C1 SDA, alternate function AF4, open-drain
    GPIOB_OTYPER |= (1 << 9);
    GPIOB_MODER &= ~(0x3 << 18);
    GPIOB_MODER |= (0x2 << 18);
    GPIOB_AFRH &= ~(0xF << 4);
    GPIOB_AFRH |= (0x4 << 4);

    //PB6 - TIM4 PWM mode output, alternate function AF2
    GPIOB_MODER &= ~(0x3 << 12);
    GPIOB_MODER |= (0x2 << 12);
    GPIOB_AFRL &= ~(0xF << 24);
    GPIOB_AFRL |= (0x2 << 24);

    // PB7 -- TIM4_CH2, alternate function AF2. Passive buzzer tone output --
    // same timer instance as PB6's LED, so it inherits whatever PSC/ARR
    // (base frequency) TIM4 is set to; only CCR2 (this channel's own duty
    // compare) is independent from PB6's CCR1.
    GPIOB_MODER &= ~(0x3 << 14);
    GPIOB_MODER |= (0x2 << 14);
    GPIOB_AFRL &= ~(0xF << 28);
    GPIOB_AFRL |= (0x2 << 28);

    // PB10 -- I2C2 SCL (slave loopback target), alternate function AF4, open-drain.
    // Pin 10 lives in AFRH (pins 8-15), at position 4*(10-8)=8.
    GPIOB_OTYPER |= (1 << 10);
    GPIOB_MODER &= ~(0x3 << 20);
    GPIOB_MODER |= (0x2 << 20);
    GPIOB_AFRH &= ~(0xF << 8);
    GPIOB_AFRH |= (0x4 << 8);

    // PB3 -- I2C2 SDA, alternate function AF9, open-drain.
    // Pin 3 lives in AFRL, not AFRH -- pins 0-7 always do, regardless of
    // which peripheral is claiming them.
    // Heads up: PB3 defaults to AF0 (JTDO/TRACESWO) at reset -- part of
    // full 5-wire JTAG, not the 2-wire SWD this board's ST-Link actually
    // uses, so reassigning it should be safe. Only PA13/PA14 are the ones
    // that must never be touched.
    GPIOB_OTYPER |= (1 << 3);
    GPIOB_MODER &= ~(0x3 << 6);
    GPIOB_MODER |= (0x2 << 6);
    GPIOB_AFRL &= ~(0xF << 12);
    GPIOB_AFRL |= (0x9 << 12);
}
