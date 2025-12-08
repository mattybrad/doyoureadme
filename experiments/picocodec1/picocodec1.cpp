#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"

#include "audio_i2s_mclk.pio.h"
#include <hardware/clocks.h>

bool reserved_addr(uint8_t addr)
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main()
{
    stdio_init_all();

    // Set up PIO for MCLK generation
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &audio_i2s_mclk_program);

    // Configure the MCLK pin
    const uint MCLK_PIN = 7;
    pio_gpio_init(pio, MCLK_PIN);
    pio_sm_set_consecutive_pindirs(pio, sm, MCLK_PIN, 1, true);

    // Calculate clock divider for 12.288 MHz
    float clk_div = clock_get_hz(clk_sys) / (12.288e6f * 2); // *2 because PIO toggles

    // Initialize the PIO state machine
    pio_sm_config c = audio_i2s_mclk_program_get_default_config(offset);
    sm_config_set_sideset_pins(&c, MCLK_PIN);
    sm_config_set_clkdiv(&c, clk_div);
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    // This example will use I2C1 on pins GP10 (SDA) and GP11 (SCL)
    i2c_init(i2c1, 100 * 1000);
    gpio_set_function(10, GPIO_FUNC_I2C);
    gpio_set_function(11, GPIO_FUNC_I2C);
    //gpio_pull_up(10);
    //gpio_pull_up(11);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(10, 11, GPIO_FUNC_I2C));

    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr)
    {
        if (addr % 16 == 0)
        {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c1, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");

    while (true) {
        printf("Hello, world!\n");
        sleep_ms(1000);
    }
}
