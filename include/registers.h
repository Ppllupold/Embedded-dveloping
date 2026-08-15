#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

//GPIOA
#define RCC_AHB1ENR (*(volatile uint32_t *)0x40023830)
#define GPIOA_MODER (*(volatile uint32_t *)0x40020000)
#define GPIOA_ODR   (*(volatile uint32_t *)0x40020014)
#define GPIOA_AFRL   (*(volatile uint32_t *)0x40020020)
// BSRR -- same atomic set/reset layout as GPIOB_BSRR below, offset 0x18
// from each port's own base (GPIOA base 0x40020000).
#define GPIOA_BSRR   (*(volatile uint32_t *)0x40020018)

//TIM2
#define RCC_APB1ENR (*(volatile uint32_t *)0x40023840)
#define TIM2_CR1 (*(volatile uint32_t *)0x40000000)
#define TIM2_PSC (*(volatile uint32_t *)0x40000028)
#define TIM2_ARR (*(volatile uint32_t *)0x4000002C)
#define TIM2_CNT (*(volatile uint32_t *)0x40000024)
#define TIM2_SR (*(volatile uint32_t *)0x40000010)
#define TIM2_DIER (*(volatile uint32_t *)0x4000000C)

//NVIC
#define NVIC_ISER0 (*(volatile uint32_t *)0xE000E100)
#define NVIC_ISER1 (*(volatile uint32_t *)0xE000E104)

// NVIC_IPRx -- priority registers, base 0xE000E400. Unlike ISERx (one bit
// per IRQ), each IPRx is 32 bits holding FOUR 8-bit priority fields, one
// per IRQ, packed byte-by-byte: IPRx covers IRQ(4x) at byte 0 up through
// IRQ(4x+3) at byte 3. IRQn's own register is IPR[n/4], at byte offset
// (n mod 4) within it. I2C1_EV_IRQn=31 -> IPR7 (31/4=7), byte 3 (31 mod 4).
// This chip only implements the TOP 4 of each byte's 8 priority bits
// (configPRIO_BITS=4 in FreeRTOSConfig.h) -- the bottom 4 are unimplemented,
// read back as 0 regardless of what's written. A priority value has to be
// shifted left by 4 within its byte to land in the bits that actually exist.
#define NVIC_IPR7  (*(volatile uint32_t *)0xE000E41C)
// I2C1_ER_IRQn=32 -> IPR8 (32/4=8), byte 0 (32 mod 4 = 0). Same packing/
// shift rules as IPR7 above.
#define NVIC_IPR8  (*(volatile uint32_t *)0xE000E420)
// EXTI4_IRQn=10 -> IPR2 (10/4=2), byte 2 (10 mod 4 = 2). Same packing/
// shift rules as IPR7/IPR8 above.
#define NVIC_IPR2  (*(volatile uint32_t *)0xE000E408)
// ADC_IRQn=18 -> IPR4 (18/4=4), byte 2 (18 mod 4 = 2). Same packing/shift
// rules as IPR2/IPR7/IPR8 above.
#define NVIC_IPR4  (*(volatile uint32_t *)0xE000E410)

//USART2
#define USART2_CR1 (*(volatile uint32_t *)0x4000440C)
#define USART2_BRR (*(volatile uint32_t *)0x40004408)
#define USART2_SR (*(volatile uint32_t *)0x40004400)
#define USART2_DR (*(volatile uint32_t *)0x40004404)

//SPI
#define RCC_APB2ENR (*(volatile uint32_t *)0x40023844)
#define SPI1_CR1 (*(volatile uint32_t *)0x40013000)
#define SPI1_SR  (*(volatile uint32_t *)0x40013008)
#define SPI1_DR  (*(volatile uint32_t *)0x4001300C)

//ADC1 -- base 0x40012000, on APB2 (RCC_APB2ENR bit 8 = ADC1EN, register already defined above)
#define ADC1_SR   (*(volatile uint32_t *)0x40012000)
#define ADC1_CR1  (*(volatile uint32_t *)0x40012004)
#define ADC1_CR2  (*(volatile uint32_t *)0x40012008)
// SMPR2 -- sample time for channels 0-9, 3 bits per channel at bit
// position 3*n. Reset default (000 = 3 cycles) is fine for low-impedance
// sources but too short for a bare resistor-divider tap -- see channel 1
// setup in test.c.
#define ADC1_SMPR2 (*(volatile uint32_t *)0x40012010)
// HTR/LTR -- analog watchdog high/low threshold registers. Always 12-bit
// fields regardless of CR1's configured resolution -- see the watchdog
// setup in test.c for how that affects the value written here.
#define ADC1_HTR   (*(volatile uint32_t *)0x40012024)
#define ADC1_LTR   (*(volatile uint32_t *)0x40012028)
#define ADC1_SQR1 (*(volatile uint32_t *)0x4001202C)
#define ADC1_SQR3 (*(volatile uint32_t *)0x40012034)
#define ADC1_DR   (*(volatile uint32_t *)0x4001204C)
// ADC_CCR is a "common" register shared across all ADC instances (clock
// prescaler, mainly) -- lives at a separate offset block, ADC1_base + 0x300,
// not among ADC1's own registers above.
#define ADC_CCR   (*(volatile uint32_t *)0x40012304)

// DMA2 -- base 0x40026400, on AHB1 (RCC_AHB1ENR, already defined above).
// Stream 0 / Channel 0 is ADC1's request-mapping slot per RM0368's DMA2
// request-mapping table -- that binding is fixed in hardware, not a choice.
//
// Design: circular mode, single buffer, half-transfer + transfer-complete
// interrupts (HTIE/TCIE) -- process each half of the buffer only while DMA
// is guaranteed to be filling the other half. DMA2_HISR/DMA2_HIFCR (streams
// 4-7's flags) and DMA2_S0M1AR (only used by double-buffer/DBM mode, not
// used here) are intentionally omitted -- no register with no job in this
// driver.
#define DMA2_LISR   (*(volatile uint32_t *)0x40026400)
#define DMA2_LIFCR  (*(volatile uint32_t *)0x40026408)
#define DMA2_S0CR   (*(volatile uint32_t *)0x40026410)
#define DMA2_S0NDTR (*(volatile uint32_t *)0x40026414)
#define DMA2_S0PAR  (*(volatile uint32_t *)0x40026418)
#define DMA2_S0M0AR (*(volatile uint32_t *)0x4002641C)
#define DMA2_S0FCR  (*(volatile uint32_t *)0x40026424)

// GPIOB -- needed for I2C1. Assuming PB8=SCL, PB9=SDA, AF4 -- the
// Nucleo-F401RE's silkscreen-labeled default I2C pins (D15/D14 on the
// Arduino header). Say now if you've actually wired different pins.
// AF4 is the fixed alternate-function slot for I2C1 on these pins per
// the datasheet's AF table -- same category as SPI1's AF5/USART2's AF7.
// AFRH (not AFRL) because pins 8-15 live in the "high" AF register --
// identical 4-bits-per-pin layout to AFRL, just the upper half.
// OTYPER is new to this project: GPIO's output *type* register, separate
// from MODER's mode field -- this is where push-pull vs open-drain gets
// selected per pin, which is exactly what I2C's shared bus requires.
#define GPIOB_MODER  (*(volatile uint32_t *)0x40020400)
// PUPDR -- pull-up/pull-down control, 2 bits per pin, same field-per-pin
// shift pattern as MODER (pin n at bits 2n/2n+1). Encoding: 00=none,
// 01=pull-up, 10=pull-down, 11=reserved.
#define GPIOB_PUPDR  (*(volatile uint32_t *)0x4002040C)
#define GPIOB_ODR    (*(volatile uint32_t *)0x40020414)
#define GPIOB_OTYPER (*(volatile uint32_t *)0x40020404)
#define GPIOB_AFRL   (*(volatile uint32_t *)0x40020420)
#define GPIOB_AFRH   (*(volatile uint32_t *)0x40020424)
// IDR -- read-only, one bit per pin, current logic level as sampled by the
// input Schmitt trigger. Needed for OneWire: reading back presence pulses
// and data bits means sampling this register while the pin sits in
// open-drain "output" mode with ODR released high -- MODER controls drive
// capability, not whether you can still read the pin.
#define GPIOB_IDR    (*(volatile uint32_t *)0x40020410)
// BSRR -- write-only, atomic set/reset, one bus write per pin change. Low
// 16 bits set the corresponding ODR bit (BS[n]), high 16 bits clear it
// (BR[n], i.e. bit (16+n)). Unlike `ODR |= ...` / `ODR &= ~...`, there's no
// read-modify-write window -- the only place in this project so far where
// that race actually matters, since OneWire's bit-bang timing is tight
// enough that even an interrupt landing between ODR's read and write could
// throw off a slot's timing.
#define GPIOB_BSRR   (*(volatile uint32_t *)0x40020418)

// I2C1 -- base 0x40005400, on APB1 (RCC_APB1ENR, already defined above).
// OAR2 (dual-address mode) and FLTR (analog/digital noise filter) are
// intentionally omitted -- not needed for a single-address master driver,
// same "no register with no job" discipline as DMA2's omissions above.
#define I2C1_CR1   (*(volatile uint32_t *)0x40005400)
#define I2C1_CR2   (*(volatile uint32_t *)0x40005404)
#define I2C1_OAR1  (*(volatile uint32_t *)0x40005408)
#define I2C1_DR    (*(volatile uint32_t *)0x40005410)
#define I2C1_SR1   (*(volatile uint32_t *)0x40005414)
#define I2C1_SR2   (*(volatile uint32_t *)0x40005418)
#define I2C1_CCR   (*(volatile uint32_t *)0x4000541C)
#define I2C1_TRISE (*(volatile uint32_t *)0x40005420)

// I2C2 -- base 0x40005800, on APB1, same register layout as I2C1 (fixed
// 0x400-byte spacing between I2Cx blocks). Used here purely as a SLAVE, to
// give I2C1's master driver a real device to find without needing an
// external part -- same "loopback proves the mechanism" idea as SPI1's
// MOSI-to-MISO jumper.
#define I2C2_CR1   (*(volatile uint32_t *)0x40005800)
#define I2C2_CR2   (*(volatile uint32_t *)0x40005804)
#define I2C2_OAR1  (*(volatile uint32_t *)0x40005808)
#define I2C2_DR    (*(volatile uint32_t *)0x40005810)
#define I2C2_SR1   (*(volatile uint32_t *)0x40005814)
#define I2C2_SR2   (*(volatile uint32_t *)0x40005818)
#define I2C2_CCR   (*(volatile uint32_t *)0x4000581C)
#define I2C2_TRISE (*(volatile uint32_t *)0x40005820)

// TIM4 -- base 0x40000800, on APB1 (RCC_APB1ENR bit 2 = TIM4EN). Same
// general-purpose timer layout as TIM2 -- CR1/PSC/ARR mean the same thing
// here, same 0x400-per-timer spacing pattern (TIM2=0x40000000,
// TIM3=0x40000400, TIM4=0x40000800). Brought up for PWM -- originally
// channel 2 (PB7), moved to channel 1 (PB6/D8) once PB7 turned out to only
// be reachable via the Morpho connector, not the Arduino header. TIM2 stays
// dedicated to its existing 1Hz tick, undisturbed either way.
// New registers for this milestone: CCMR1 (Capture/Compare Mode Register 1 --
// packs channel 1 AND channel 2's mode config into one 32-bit register, two
// 8-bit halves), CCER (Capture/Compare Enable Register -- actually connects
// a channel's comparator output to the physical pin, plus polarity), and
// CCR1/CCR2 (each channel's own compare value -- this is what sets duty
// cycle). CCMR2 (channels 3/4) and CCR3/CCR4 intentionally omitted -- only
// channels 1 and 2 have ever had a job in this driver.
#define TIM4_CR1   (*(volatile uint32_t *)0x40000800)
#define TIM4_CCMR1 (*(volatile uint32_t *)0x40000818)
#define TIM4_CCER  (*(volatile uint32_t *)0x40000820)
#define TIM4_PSC   (*(volatile uint32_t *)0x40000828)
#define TIM4_ARR   (*(volatile uint32_t *)0x4000082C)
#define TIM4_CCR1  (*(volatile uint32_t *)0x40000834)
#define TIM4_CCR2  (*(volatile uint32_t *)0x40000838)

// SYSCFG -- base 0x40013800, on APB2 (RCC_APB2ENR, already defined above --
// bit 14 = SYSCFGEN). Its only job in this driver is the EXTI port mux:
// EXTI has just 16 lines (one per pin NUMBER, 0-15), shared across every
// port, so this register picks which port feeds a given line. Only
// EXTICR2 is defined here -- it covers lines 4-7, and PB4 (flame sensor
// DO) is the only EXTI source this project uses so far. EXTICR1/3/4 would
// cover lines 0-3/8-11/12-15 if another EXTI source gets added later.
#define SYSCFG_EXTICR2 (*(volatile uint32_t *)0x4001380C)

// EXTI -- base 0x40013C00, on APB2. Sits logically between SYSCFG's port
// mux and the NVIC: IMR is the "arm this line to actually request an
// interrupt" bit (same role as TIM2_DIER's UIE), RTSR/FTSR independently
// select rising/falling edge sensitivity per line (new -- nothing before
// this had edge-selectable triggering), and PR is the pending flag --
// write-1-to-clear, same convention as DMA2_LIFCR, NOT TIM2_SR's UIF-style
// direct &=~bit clear. EMR (event, not interrupt, mask register) and
// SWIER (software interrupt event register) are intentionally omitted --
// no register with no job in this driver.
#define EXTI_IMR  (*(volatile uint32_t *)0x40013C00)
#define EXTI_RTSR (*(volatile uint32_t *)0x40013C08)
#define EXTI_FTSR (*(volatile uint32_t *)0x40013C0C)
#define EXTI_PR   (*(volatile uint32_t *)0x40013C14)

// Shared between TIM2_IRQHandler (startup.c) and main() (test.c).
// ISR just sets this flag -- it does NOT touch USART2_DR itself, keeping
// the ISR body short and deterministic. main() does the actual byte-by-byte
// TXE-wait send, where blocking is acceptable.
extern volatile int tick_flag;
extern volatile int rx_ready_flag;
extern volatile char received_char;

extern volatile uint8_t dma_sample_buffer[20];
extern volatile int half_sample;

#endif // REGISTERS_H