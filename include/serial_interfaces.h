#ifndef SERIAL_INTERFACES_H
#define SERIAL_INTERFACES_H

#include "registers.h"
#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

// One state per hardware wait-point in the I2C1 master transaction --
// each ISR case reacts to exactly one event and performs the action that
// produces the *next* event. State advances only inside the flag-check,
// never unconditionally.
typedef enum {
    I2C_STATE_IDLE,
    I2C_STATE_GENERATE_START,
    I2C_STATE_ADDR_WRITE_ACK,
    I2C_STATE_WRITE_COMPLETE,
    I2C_STATE_REPEATED_START,
    I2C_STATE_ADDR_READ_ACK,
    I2C_STATE_READ_AVAILABLE,
    I2C_STATE_DONE
} i2c_bus_state_t;

extern volatile i2c_bus_state_t i2c_state;
extern volatile uint8_t i2c_received_data;
extern volatile uint8_t i2c_sended_data;
extern volatile uint32_t current_i2c_address;
extern volatile int i2c_last_ok; // set by the ISR: 1 = real ACK'd transfer, 0 = AF-aborted
extern TaskHandle_t xI2CTaskHandle; // set by xTaskCreate() when vI2CTask is created
extern TaskHandle_t xFlameTaskHandle; // set by xTaskCreate() when vFlameAlarmTask is created --
                                       // EXTI4_IRQHandler notifies this, same role as xI2CTaskHandle
extern TaskHandle_t xLaserTaskHandle; // set by xTaskCreate() when vLaserTripTask is created --
                                       // ADC_IRQHandler notifies this, same role as xFlameTaskHandle
extern TaskHandle_t xPWMTaskHandle; // set by xTaskCreate() when vPWMTimerTask is created --
                                     // notified from vLaserTripTask (task context, not an ISR),
                                     // so this one uses plain xTaskNotify, not the FromISR variant

// USART2
void usart2_write_byte(char byte);
void usart2_write_uint16(uint16_t value);
void usart2_write_float(float value, uint8_t decimals);

// SPI1
uint8_t spi1_transfer_byte(uint8_t data);
// ADC1
uint16_t adc1_read(void);

// I2C1
void i2c1_scan_bus(void);          // old, plain-polling version -- kept as the known-good reference
int i2c1_start_transaction(uint8_t address);
void i2c1_scan_bus_interrupt(void); // new, interrupt-driven version

#endif // SERIAL_INTERFACES_H
