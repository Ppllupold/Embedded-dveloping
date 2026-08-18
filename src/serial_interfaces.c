#include <stdio.h>
#include "serial_interfaces.h"

// =======================================================================
// USART2
// =======================================================================
static int get_int_len(int number);

void usart2_write_byte(char byte)
{
    // Wait until TXE (Transmit Data Register Empty) is set
    while (!((USART2_SR >> 7) & 1))
    {
    }
    USART2_DR = byte; // Write the byte to the data register
}

// =======================================================================
// SPI1
// =======================================================================

// SPI is inherently a combined send-and-receive operation (full-duplex
// shift-register loop), so unlike UART's separate TX/RX paths, one
// transfer function both writes and reads. SPI1_SR's bit layout is a
// different shape from USART2_SR -- RXNE is bit 0, TXE is bit 1 here,
// not 5/7 like USART2.
uint8_t spi1_transfer_byte(uint8_t data)
{
    while (!((SPI1_SR >> 1) & 1))
    {
    } // wait for TXE -- safe to load the next outgoing byte

    SPI1_DR = data; // writing DR starts the shift-out on MOSI

    while (!((SPI1_SR >> 0) & 1))
    {
    } // wait for RXNE -- the shifted-in byte on MISO is ready

    return SPI1_DR & 0xFF; // reading DR also clears RXNE
}

uint16_t adc1_read(void)
{
    ADC1_CR2 |= (1 << 30); // Start conversion

    while (!(ADC1_SR & (1 << 1))) {} // Waiting for the end of conversion

    uint16_t data = ADC1_DR & 0xFFF;

    return data;
}

void usart2_write_uint16(uint16_t value) {

    int digit_length = get_int_len(value);
    char message[6];
    for (int i = 0; i < digit_length; i++) {
        int fraction = value % 10;
        message[digit_length - 1 - i] = (char)('0' + fraction);
        value /= 10;
    }
    for (int i = 0; i < digit_length; i++) {
        usart2_write_byte(message[i]);
    }
}

// Generic float printer -- digit-by-digit fractional extraction sidesteps
// the leading-zero problem usart2_write_uint16() has (that function counts
// significant digits, so a fractional remainder like 625 would print as
// "625" instead of "0625"). Multiplying the remaining fraction by 10 each
// pass and taking the integer part emits exactly one digit per iteration,
// leading zeros included by construction, for as many decimal places as
// the caller asks for.
void usart2_write_float(float value, uint8_t decimals) {
    if (value < 0.0f) {
        usart2_write_byte('-');
        value = -value;
    }

    uint16_t whole = (uint16_t)value;
    usart2_write_uint16(whole);

    if (decimals == 0) {
        return;
    }

    usart2_write_byte('.');

    float frac = value - (float)whole;
    for (uint8_t i = 0; i < decimals; i++) {
        frac *= 10.0f;
        uint8_t digit = (uint8_t)frac;
        usart2_write_byte((char)('0' + digit));
        frac -= (float)digit;
    }
}

// Kicks off one transaction and returns immediately -- the actual clocking
// happens in I2C1_EV_IRQHandler from here on. Only starts if the bus isn't
// mid-transaction already (IDLE or DONE from a previous one).
int i2c1_start_transaction(uint8_t address)
{
    if (i2c_state != I2C_STATE_IDLE && i2c_state != I2C_STATE_DONE)
    {
        return 0;
    }
    current_i2c_address = address;
    i2c_state = I2C_STATE_GENERATE_START;
    I2C1_CR1 |= (1 << 8); // Generate START
    return 1;
}

// Interrupt-driven sweep, same address range and same "only print on real
// ACK" behavior as the polling version below -- but each address's actual
// bit-banging happens inside I2C1_EV_IRQHandler, not this loop. The
// while-wait here is a plain busy-wait (CPU isn't doing anything else in
// this bare-metal build anyway), same flag-poll idiom as tick_flag/
// rx_ready_flag elsewhere in this project -- just waiting on i2c_state
// instead of a dedicated flag, since the state machine's own DONE value
// already means exactly that.
void i2c1_scan_bus_interrupt(void)
{
    for (uint8_t address = 0x08; address <= 0x77; address++)
    {
        if (i2c1_start_transaction(address))
        {
            while (i2c_state != I2C_STATE_DONE) {}

            if (i2c_last_ok)
            {
                usart2_write_uint16(i2c_sended_data);
                usart2_write_uint16(i2c_received_data);
                usart2_write_byte('\r');
                usart2_write_byte('\n');
            }
        }
    }
}

// Scans the conventional 7-bit address range (0x08-0x77 -- 0x00-0x07 and
// 0x78-0x7F are reserved for special protocol purposes, not real devices),
// printing every address that ACKs. One STOP per address, whether it
// ACKed or not, so each attempt starts the bus fresh.
void i2c1_scan_bus(void)
{
    for (uint8_t address = 0x08; address <= 0x77; address++)
    {
        I2C1_CR1 |= (1 << 8);             // Generate START
        while (!((I2C1_SR1 >> 0) & 1)) {} // Wait for SB

        I2C1_DR = (uint8_t)(( address << 1) | 0); // address byte, write direction

        while (1)
        {
            if ((I2C1_SR1 >> 1) & 1)
            {
                // ADDR set -- something acknowledged this address. Reading
                // SR1 just now (as part of this very check) is half of
                // ADDR's clear sequence; reading SR2 completes it.
                (void)I2C1_SR2;

                // I2C2 (slave) sets its OWN ADDR flag on this same match,
                // and with clock stretching enabled by default (NOSTRETCH=0),
                // it holds SCL low until that flag is serviced -- which
                // blocks our upcoming STOP (needs SCL high) and every
                // address after this one. A real slave would clear this
                // from its own ISR; here, since master and slave run
                // synchronously on one core, this loop has to do it on
                // I2C2's behalf. Same clear sequence as I2C1's own ADDR:
                // read SR1 then SR2.
                (void)I2C2_SR1;
                (void)I2C2_SR2;
                I2C1_DR = 'A';

                while (((I2C2_SR1 >> 6) & 1) == 0) {} // rxne wait
                uint8_t sended = I2C2_DR & 0xFF; //capture the sended data
                while (((I2C1_SR1 >> 2) & 1) == 0) {} // btf wait
                I2C1_CR1 |= (1 << 8); //repeated start
                while (!((I2C1_SR1 >> 0) & 1)) {} // sb wait
                I2C1_DR = (uint8_t)(( address << 1) | 1); // address byte, read direction
                while (!((I2C1_SR1 >> 1) & 1)) {} // ADDR wait
                I2C1_CR1 &= ~(1 << 10); //clear ACK
                (void)I2C1_SR2;     //clear ADDRS
                (void)I2C2_SR1;
                (void)I2C2_SR2;

                I2C1_CR1 |= (1 << 9); // Generate STOP -- release the bus before the next attempt
                I2C2_DR = 'E';
                while (((I2C1_SR1 >> 6) & 1) == 0) {}
                uint8_t received = I2C1_DR & 0xFF;

                usart2_write_uint16(sended);
                usart2_write_uint16(received);
                usart2_write_byte('\r');
                usart2_write_byte('\n');
                break;
            }
            if ((I2C1_SR1 >> 10) & 1)
            {
                // AF set -- nobody answered. Clears differently from ADDR:
                // software writes the bit back to 0 directly, no SR2 read.
                I2C1_SR1 &= ~(1 << 10);
                I2C1_CR1 |= (1 << 9);
                break;
            }
        }

    }
}

static int get_int_len(int number) {
    if (number > 65536) {return 0;}
    int count = 0;

    do
    {
        count++;
        number /= 10;
    } while (number != 0);
    return count;
    
}
