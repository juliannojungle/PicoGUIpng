#ifndef _PLATFORM_C_
#define _PLATFORM_C_

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "stdio.h"
#include "hardware/i2c.h"
#include "hardware/pwm.h"

#define UBYTE uint8_t
#define UWORD uint16_t
#define UDOUBLE uint32_t

#define SPI_PORT spi1

void DigitalWrite(uint pin, UBYTE value) {
    gpio_put(pin, value);
}

UBYTE DigitalRead(uint pin) {
    return gpio_get(pin);
}

void SPIWriteByte(UBYTE value) {
    spi_write_blocking(SPI_PORT, &value, 1);
}

void SPIWriteNByte(UBYTE pData[], UDOUBLE len) {
    spi_write_blocking(SPI_PORT, pData, len);
}

void I2CWrite(UBYTE addr, UBYTE reg, UBYTE value) {
    UBYTE data[2] = { reg, value };
    i2c_write_blocking(i2c1, addr, data, 2, false);
}

void I2CWriteNByte(UBYTE addr, UBYTE *pData, UDOUBLE len) {
    i2c_write_blocking(i2c1, addr, pData, len, false);
}

UBYTE I2CReadByte(UBYTE addr, UBYTE reg) {
    UBYTE buf;
    i2c_write_blocking(i2c1, addr, &reg, 1, true);
    i2c_read_blocking(i2c1, addr, &buf, 1, false);
    return buf;
}

void GPIOInit(uint pin) {
    gpio_init(pin);
}

void GPIOSetDir(uint pin, uint mode) {
    gpio_set_dir(pin, mode);
}

void GPIOPullUp(uint pin) {
    gpio_pull_up(pin);
}

void Delay(uint milliseconds) {
    sleep_ms(milliseconds);
}

void STDIOInitAll(void) {
    stdio_init_all();
}

void SPIInit(uint speed) {
    spi_init(SPI_PORT, speed);
}

void GPIOSetFunction(uint pin, uint function) {
    gpio_set_function(pin, (uint)function);
}

uint PWMGPIOToSliceNum(uint pin) {
    return pwm_gpio_to_slice_num(pin);
}

void PWMSetWrap(uint slice, uint value) {
    pwm_set_wrap(slice, value);
}

void PWMSetChannelLevel(uint slice, uint channel, UWORD level) {
    pwm_set_chan_level(slice, (uint)channel, level);
}

void PWMSetClockDivider(uint slice, float divider) {
    pwm_set_clkdiv(slice, divider);
}

void PWMSetEnabled(uint slice, bool enable) {
    pwm_set_enabled(slice, enable);
}

void I2CInit(uint speed) {
    i2c_init(i2c1, speed);
}

#endif