#ifndef IRQ_H
#define IRQ_H

// Core-level interrupt masking (PRIMASK) -- not tied to any specific
// peripheral, unlike usart2.h/usart2.c. Every future driver (SPI, I2C, ...)
// will want these same two functions, so they live in their own header
// rather than being owned by any one peripheral's driver file.
//
// static inline: no matching .c file needed. "static" gives each
// translation unit its own private copy (internal linkage), sidestepping
// C's notoriously tricky rules around plain `inline` external linkage.
// "inline" is a hint that lets the compiler paste the CPSID/CPSIE
// instructions directly at the call site instead of paying real call/return
// overhead -- important here, since every extra cycle between disable and
// enable directly extends how long every other interrupt in the system
// stays masked.

static inline void disable_irq(void)
{
    __asm volatile ("cpsid i" ::: "memory");
}

static inline void enable_irq(void)
{
    __asm volatile ("cpsie i" ::: "memory");
}

#endif // IRQ_H
