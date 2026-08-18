#ifndef ONEWIRE_H
#define ONEWIRE_H

#include <stdint.h>

// TIM2 is repurposed as a free-running, interrupt-free microsecond counter
// (see test.c: PSC=15, ARR=0xFFFFFFFF) specifically to give this driver a
// hardware-backed clock for delays -- immune to compiler/flash-wait-state
// variance the way a busy-wait cycle-count loop wouldn't be.
void delay_us(uint32_t us);

// Bit-bang primitives, all driving PB0 (open-drain, external 4.7k not
// needed here -- the DS18B20 module has its own onboard pull-up).
int onewire_reset(void);      // 1 = device present, 0 = nothing answered
void write_bit(int bit);
int read_bit(void);
void write_byte(uint8_t byte);
uint8_t read_byte(void);

// Full DS18B20 conversation: reset -> Skip ROM -> Convert T -> wait ->
// reset -> Skip ROM -> Read Scratchpad -> print over USART2.
void ds18b20_read_temperature(void);

#endif // ONEWIRE_H
