#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/pio.h"
#include "hardware/uart.h"
#include "hardware/i2c.h"

#include "audio_i2s_mclk.pio.h"
#include <hardware/clocks.h>
#include <stdlib.h>
#include <math.h>

bool reserved_addr(uint8_t addr)
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

// SGTL5000 I2C address and register definitions
#define SGTL5000_I2C_ADDR       0x0A
#define SGTL5000_CHIP_ID        0x0000
#define SGTL5000_CHIP_DIG_POWER 0x0002
#define SGTL5000_CHIP_CLK_CTRL  0x0004
#define SGTL5000_CHIP_I2S_CTRL  0x0006
#define SGTL5000_CHIP_SSS_CTRL  0x000A
#define SGTL5000_CHIP_ADCDAC_CTRL 0x000E
#define SGTL5000_CHIP_DAC_VOL   0x0010
#define SGTL5000_CHIP_PAD_STRENGTH 0x0014
#define SGTL5000_CHIP_ANA_POWER 0x0030
#define SGTL5000_CHIP_LINREG_CTRL 0x0032
#define SGTL5000_CHIP_REF_CTRL  0x0034
#define SGTL5000_CHIP_MIC_CTRL  0x0036
#define SGTL5000_CHIP_LINE_OUT_CTRL 0x0038
#define SGTL5000_CHIP_LINE_OUT_CTRL 0x002C
#define SGTL5000_CHIP_LINE_OUT_VOL 0x002E
#define SGTL5000_CHIP_ANA_HP_CTRL 0x0022
#define SGTL5000_CHIP_ANA_CTRL  0x0024
#define SGTL5000_CHIP_SHORT_CTRL 0x0026

// Write 16-bit register to SGTL5000
bool sgtl5000_write_reg(uint16_t reg, uint16_t val) {
    uint8_t data[4];
    data[0] = reg >> 8;    // Register high byte
    data[1] = reg & 0xFF;  // Register low byte  
    data[2] = val >> 8;    // Value high byte
    data[3] = val & 0xFF;  // Value low byte
    
    int ret = i2c_write_blocking(i2c1, SGTL5000_I2C_ADDR, data, 4, false);
    return ret == 4;
}

// Read 16-bit register from SGTL5000
uint16_t sgtl5000_read_reg(uint16_t reg) {
    uint8_t reg_data[2];
    uint8_t val_data[2];
    
    reg_data[0] = reg >> 8;
    reg_data[1] = reg & 0xFF;
    
    i2c_write_blocking(i2c1, SGTL5000_I2C_ADDR, reg_data, 2, true);  // Keep bus control
    i2c_read_blocking(i2c1, SGTL5000_I2C_ADDR, val_data, 2, false);
    
    return (val_data[0] << 8) | val_data[1];
}

// Initialize SGTL5000 for I2S playback (Teensy Audio Board compatible)
bool sgtl5000_init() {
    printf("Initializing SGTL5000 (Teensy Audio Board config)...\n");
    
    sleep_ms(5);  // Initial delay as in Teensy code
    
    // Check chip ID
    uint16_t chip_id = sgtl5000_read_reg(SGTL5000_CHIP_ID);
    printf("SGTL5000 Chip ID: 0x%04X\n", chip_id);
    if ((chip_id >> 8) != 0xA0) {
        printf("ERROR: Invalid chip ID!\n");
        return false;
    }
    
    // Teensy Audio Board initialization sequence
    // Step 1: Power up analog - VDDD externally driven with 1.8V
    if (!sgtl5000_write_reg(SGTL5000_CHIP_ANA_POWER, 0x4060)) {
        printf("ERROR: Failed to write ANA_POWER!\n");
        return false;
    }
    
    // Step 2: Configure voltage regulators - VDDA & VDDIO both over 3.1V  
    sgtl5000_write_reg(SGTL5000_CHIP_LINREG_CTRL, 0x006C);
    sgtl5000_write_reg(SGTL5000_CHIP_REF_CTRL, 0x01F2);
    sgtl5000_write_reg(SGTL5000_CHIP_LINE_OUT_CTRL, 0x0322);
    sgtl5000_write_reg(SGTL5000_CHIP_SHORT_CTRL, 0x1106);
    sgtl5000_write_reg(SGTL5000_CHIP_ANA_CTRL, 0x0137);
    
    // Step 3: Power up all analog blocks for I2S slave mode
    sgtl5000_write_reg(SGTL5000_CHIP_ANA_POWER, 0x40FF);  // power up: lineout, hp, adc, dac
    
    // Step 4: Power up all digital stuff  
    sgtl5000_write_reg(SGTL5000_CHIP_DIG_POWER, 0x0073);
    sleep_ms(400);  // Important delay for power stabilization
    
    // Step 5: Configure line out volume (default ~1.3V peak-to-peak)
    sgtl5000_write_reg(SGTL5000_CHIP_LINE_OUT_VOL, 0x1D1D);
    
    // Step 6: Configure for I2S slave mode, 48kHz, 256*Fs
    sgtl5000_write_reg(SGTL5000_CHIP_CLK_CTRL, 0x0004);      // 256*Fs, 48kHz  
    sgtl5000_write_reg(SGTL5000_CHIP_I2S_CTRL, 0x0030);      // SCLK=64*Fs, 16bit, I2S format, slave mode
    
    // Step 7: Route I2S input to DAC to headphones
    sgtl5000_write_reg(SGTL5000_CHIP_SSS_CTRL, 0x0010);      // DAC select I2S_IN
    sgtl5000_write_reg(SGTL5000_CHIP_ADCDAC_CTRL, 0x0000);   // Default settings
    
    // Step 8: Set reasonable volumes
    sgtl5000_write_reg(SGTL5000_CHIP_DAC_VOL, 0x3C3C);       // 0dB DAC volume
    sgtl5000_write_reg(SGTL5000_CHIP_ANA_HP_CTRL, 0x1818);   // Reasonable headphone volume
    
    printf("SGTL5000 initialization complete (Teensy Audio Board style)!\n");
    return true;
}

int main()
{
    stdio_init_all();

    // Set up PIO for SGTL5000 clock generation
    PIO pio = pio0;
    
    // Define clock pins
    const uint MCLK_PIN = 7;   // 12.288MHz master clock
    const uint LRCLK_PIN = 8;  // 48kHz word select (L/R)
    const uint BCLK_PIN = 9;   // 1.536MHz bit clock  
    
    // Claim three state machines
    uint mclk_sm = pio_claim_unused_sm(pio, true);
    uint lrclk_sm = pio_claim_unused_sm(pio, true);
    uint bclk_sm = pio_claim_unused_sm(pio, true);
    
    // Add the programs and get their offsets
    uint mclk_offset = pio_add_program(pio, &audio_mclk_program);
    uint bclk_offset = pio_add_program(pio, &audio_bclk_program);
    uint lrclk_offset = pio_add_program(pio, &audio_lrclk_program);
    
    // Initialize each clock generator
    audio_mclk_program_init(pio, mclk_sm, mclk_offset, MCLK_PIN);
    audio_bclk_program_init(pio, bclk_sm, bclk_offset, BCLK_PIN);
    audio_lrclk_program_init(pio, lrclk_sm, lrclk_offset, LRCLK_PIN);
    
    // Start all clocks synchronized
    audio_clocks_start(pio, mclk_sm, bclk_sm, lrclk_sm);
    
    // Set up I2S data transmitter
    const uint I2S_DATA_PIN = 12;  // I2S data output
    uint i2s_sm = pio_claim_unused_sm(pio, true);
    uint i2s_offset = pio_add_program(pio, &audio_i2s_tx_program);
    audio_i2s_tx_program_init(pio, i2s_sm, i2s_offset, I2S_DATA_PIN, LRCLK_PIN);
    audio_i2s_start(pio, i2s_sm);
    
    printf("SGTL5000 clocks started:\n");
    printf("  System Clock: %lu Hz\n", clock_get_hz(clk_sys));
    printf("  MCLK:  12.288MHz on pin %d\n", MCLK_PIN);
    printf("  BCLK:  1.536MHz on pin %d (target frequency)\n", BCLK_PIN);
    printf("  LRCLK: 48kHz on pin %d\n", LRCLK_PIN);
    printf("  I2S DATA: pin %d (test tone)\n", I2S_DATA_PIN);

    // This example will use I2C1 on pins GP10 (SDA) and GP11 (SCL)
    i2c_init(i2c1, 100 * 1000);
    gpio_set_function(10, GPIO_FUNC_I2C);
    gpio_set_function(11, GPIO_FUNC_I2C);
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

    // Initialize SGTL5000 audio codec
    if (!sgtl5000_init()) {
        printf("SGTL5000 initialization failed!\n");
        return -1;
    }

    printf("Starting white noise generation...\n");

    // Seed random number generator
    srand(time_us_32());
    
    // Test with simple tone instead of white noise for easier debugging
    uint32_t sample_counter = 0;
    bool debug_mode = true;
    
    while (true) {
        if (debug_mode && sample_counter % 48000 == 0) { // Print every second
            printf("Sending audio samples... Count: %lu\n", sample_counter);
        }
        
        // Generate simple test tone (1kHz sine wave) instead of white noise
        float t = (float)sample_counter / 48000.0f;
        float sine_wave = sinf(2.0f * 3.14159f * 1000.0f * t) * 16000.0f; // 1kHz tone
        int16_t test_sample = (int16_t)sine_wave;
        
        // Use same sample for left and right channels
        uint32_t stereo_sample = ((uint32_t)(test_sample & 0xFFFF) << 16) | (test_sample & 0xFFFF);
        
        // Send to I2S transmitter PIO (blocks if FIFO is full)
        pio_sm_put_blocking(pio, i2s_sm, stereo_sample);
        
        sample_counter++;
    }
}
