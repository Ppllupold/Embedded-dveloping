#include "onewire.h"
#include "registers.h"
#include "serial_interfaces.h"
#include "FreeRTOS.h"
#include "task.h"

void delay_us(uint32_t us)
{
    uint32_t start = TIM2_CNT;
    while ((TIM2_CNT - start) < us) {}
}

// Reset + presence pulse. PB5 stays in open-drain output mode throughout --
// reading GPIOB_IDR works regardless of MODER, since the input Schmitt
// trigger is always live, output mode or not.
int onewire_reset(void)
{
    GPIOB_BSRR = (1 << 21); // drive PB5 low
    delay_us(500);
    GPIOB_BSRR = (1 << 5);  // release PB5 high
    delay_us(70);
    uint32_t line = (GPIOB_IDR >> 5) & 1;
    delay_us(400); // let the rest of the presence window finish before returning
    return !line;   // line low = a device pulled it down = presence detected
}

void write_bit(int bit)
{
    if (bit)
    {
        GPIOB_BSRR = (1 << 21);
        delay_us(6);
        GPIOB_BSRR = (1 << 5);
        delay_us(54);
    }
    else
    {
        GPIOB_BSRR = (1 << 21);
        delay_us(60);
        GPIOB_BSRR = (1 << 5);
        delay_us(1); // explicit recovery -- same guaranteed shape as the bit==1 path,
                     // not left to incidental instruction timing
    }
}

int read_bit(void)
{
    GPIOB_BSRR = (1 << 21);
    delay_us(3); // short master-initiated pulse -- keeps drive+wait comfortably
                 // under the 15us window during which the device's answer
                 // is guaranteed valid
    GPIOB_BSRR = (1 << 5);
    delay_us(25);
    int line = (GPIOB_IDR >> 5) & 1;
    delay_us(32);
    return line; // raw pin level *is* the bit here, unlike reset()'s presence signal
}

void write_byte(uint8_t byte)
{
    for (int i = 0; i < 8; i++) // LSB first -- opposite of I2C's MSB-first convention
    {
        write_bit((byte >> i) & 1);
    }
}

uint8_t read_byte(void)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++)
    {
        if (read_bit())
        {
            byte |= (1 << i);
        }
    }
    return byte;
}

// Workaround, not a fix: this specific chip runs on a razor-thin voltage
// margin (measured ~3.5V against a real ~3.0V minimum, likely a damaged
// internal component eating into headroom that should exist), and shows
// intermittent, non-deterministic presence-pulse failures as a result --
// retrying gives it several chances to catch it on a good cycle instead
// of failing the whole read over one missed presence pulse. A healthy
// chip wouldn't need this at all.
static int onewire_reset_retry(uint8_t attempts)
{
    for (uint8_t i = 0; i < attempts; i++)
    {
        if (onewire_reset())
        {
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(20)); // brief real-world pause between attempts
    }
    return 0;
}

// Skip ROM is safe here because there's exactly one device on the bus --
// a real multi-device bus would need the ROM search algorithm instead.
// The 750ms wait is a plain vTaskDelay, not a busy-wait: unlike the
// microsecond-scale bit timing above, this is FreeRTOS-tick-granularity
// long, so there's no reason to block the CPU instead of letting the
// scheduler run other tasks during the conversion.
void ds18b20_read_temperature(void)
{
    if (!onewire_reset_retry(10))
    {
        usart2_write_byte('X'); // no device answered
        usart2_write_byte('\r');
        usart2_write_byte('\n');
        return;
    }

    write_byte(0xCC); // Skip ROM
    write_byte(0x44); // Convert T

    vTaskDelay(pdMS_TO_TICKS(750)); // worst-case 12-bit conversion time

    if (!onewire_reset_retry(10))
    {
        usart2_write_byte('X');
        usart2_write_byte('\r');
        usart2_write_byte('\n');
        return;
    }

    write_byte(0xCC); // Skip ROM
    write_byte(0xBE); // Read Scratchpad

    uint8_t lsb = read_byte();
    uint8_t msb = read_byte();

    uint16_t raw_u = (uint16_t)((msb << 8) | lsb);
    int16_t raw_temp = (int16_t)raw_u; // explicit reinterpret, not implicit narrowing

    float temp_c = raw_temp / 16.0f;

    usart2_write_float(temp_c, 4);
    usart2_write_byte('\r');
    usart2_write_byte('\n');
}
