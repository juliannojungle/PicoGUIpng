/*****************************************************************************
* | File          :   DEV_Config.c
* | Author      :   
* | Function    :   Hardware underlying interface
* | Info        :
*----------------
* |    This version:   V1.0
* | Date        :   2021-03-16
* | Info        :   
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of theex Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
******************************************************************************/
#ifndef _DRIVER_C_
#define _DRIVER_C_

// #include "Driver.h"

// GPIO
int EPD_RST_PIN;
int EPD_DC_PIN;
int EPD_CS_PIN;
int EPD_BL_PIN;
int EPD_CLK_PIN;
int EPD_MOSI_PIN;
int EPD_SCL_PIN;
int EPD_SDA_PIN;

uint slice_num;

void DriverGPIOMode(UWORD pin, UWORD mode) {
    GPIOInit(pin);

    if(mode == 0 || mode == GPIO_IN) {
        GPIOSetDir(pin, GPIO_IN);
    } else {
        GPIOSetDir(pin, GPIO_OUT);
    }
}

void DriverGPIOInit(void) {
    DriverGPIOMode(EPD_RST_PIN, 1);
    DriverGPIOMode(EPD_DC_PIN, 1);
    DriverGPIOMode(EPD_CS_PIN, 1);
    DriverGPIOMode(EPD_BL_PIN, 1);

    DriverGPIOMode(EPD_CS_PIN, 1);
    DriverGPIOMode(EPD_BL_PIN, 1);

    DigitalWrite(EPD_CS_PIN, 1);
    DigitalWrite(EPD_DC_PIN, 0);
    DigitalWrite(EPD_BL_PIN, 1);
}

UBYTE DriverInit(void) {
    STDIOInitAll();

    //GPIO PIN
    EPD_RST_PIN = 12;
    EPD_DC_PIN = 8;
    EPD_BL_PIN = 13;
    EPD_CS_PIN = 9;
    EPD_CLK_PIN = 10;
    EPD_MOSI_PIN = 11;
    EPD_SCL_PIN = 7;
    EPD_SDA_PIN = 6;

    // SPI Config
    SPIInit(10000 * 1000);
    GPIOSetFunction(EPD_CLK_PIN, GPIO_FUNC_SPI);
    GPIOSetFunction(EPD_MOSI_PIN, GPIO_FUNC_SPI);

    // GPIO Config
    DriverGPIOInit();

    // PWM Config
    GPIOSetFunction(EPD_BL_PIN, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(EPD_BL_PIN);
    PWMSetWrap(slice_num, 100);
    PWMSetChannelLevel(slice_num, PWM_CHAN_B, 1);
    PWMSetClockDivider(slice_num, 50);
    PWMSetEnabled(slice_num, true);

    //I2C Config
    I2CInit(300*1000);
    GPIOSetFunction(EPD_SDA_PIN, GPIO_FUNC_I2C);
    GPIOSetFunction(EPD_SCL_PIN, GPIO_FUNC_I2C);
    GPIOPullUp(EPD_SDA_PIN);
    GPIOPullUp(EPD_SCL_PIN);

    printf("DriverInit OK\r\n");
    return 0;
}

void DriverSetPWM(UBYTE value) {
    if ((value < 0) || (value > 100)) {
        printf("DriverSetPWM error\r\n");
    } else {
        PWMSetChannelLevel(slice_num, 1, value);
    }
}

void DriverExit(void) {
}

#endif