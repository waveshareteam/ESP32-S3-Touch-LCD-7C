/*****************************************************************************
 * | File         :   io_extension.cpp
 * | Author       :   Waveshare team
 * | Function     :   IO_EXTENSION GPIO control via I2C interface
 * | Info         :
 * |                 I2C driver code for controlling GPIO pins using IO_EXTENSION chip.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-27
 * | Info         :   Basic version, includes functions to read and write
 * |                 GPIO pins using I2C communication with IO_EXTENSION.
 *
 ******************************************************************************/
#include "io_extension.h"

io_extension_obj_t IO_EXTENSION;

void IO_EXTENSION_SetAllMode(uint16_t mode_mask)
{
    IO_EXTENSION.Mode_value = mode_mask;

    uint8_t data[3];
    data[0] = IO_EXTENSION_Mode;
    data[1] = (uint8_t)(mode_mask & 0xFF);
    data[2] = (uint8_t)((mode_mask >> 8) & 0xFF);

    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 3);
}

void IO_EXTENSION_IO_Mode(uint8_t pin, uint8_t mode)
{
    if (pin > 15) return;

    if (mode)
        IO_EXTENSION.Mode_value |= (1U << pin);
    else
        IO_EXTENSION.Mode_value &= ~(1U << pin);

    IO_EXTENSION_SetAllMode(IO_EXTENSION.Mode_value);
}

void IO_EXTENSION_Init()
{
    DEV_I2C_Set_Slave_Addr(&IO_EXTENSION.addr, IO_EXTENSION_ADDR);
    IO_EXTENSION_SetAllMode(0xFFFF);
    IO_EXTENSION.Last_io_value = 0xFFFF;
}

void IO_EXTENSION_Output(uint8_t pin, uint8_t value)
{
    if (pin > 15) return;

    uint16_t mask = (uint16_t)(1U << pin);
    IO_EXTENSION.Last_io_value = value ?
        (IO_EXTENSION.Last_io_value | mask) :
        (IO_EXTENSION.Last_io_value & (uint16_t)(~mask));

    uint8_t data[3];
    data[0] = IO_EXTENSION_IO_OUTPUT_ADDR;
    data[1] = (uint8_t)(IO_EXTENSION.Last_io_value & 0xFF);
    data[2] = (uint8_t)((IO_EXTENSION.Last_io_value >> 8) & 0xFF);

    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 3);
}

uint8_t IO_EXTENSION_Input(uint8_t pin)
{
    if (pin > 15) return 0;

    uint8_t buf[2] = {0};
    uint16_t value = 0;

    DEV_I2C_Read_Nbyte(IO_EXTENSION.addr, IO_EXTENSION_IO_INPUT_ADDR, buf, 2);
    value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);

    return (value & (1U << pin)) ? 1 : 0;
}

void IO_EXTENSION_Pwm_Output(uint8_t value)
{
    if (value <= 5)
    {
        value = 5;
    }

    uint8_t data[2] = {IO_EXTENSION_PWM_ADDR, value};
    data[1] = (uint8_t)(value * (255 / 100.0f));
    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 2);
}

uint16_t IO_EXTENSION_Adc_Input()
{
    return DEV_I2C_Read_Word(IO_EXTENSION.addr, IO_EXTENSION_ADC_ADDR);
}

uint8_t IO_EXTENSION_RTC_INT_READ()
{
    return (uint8_t)DEV_I2C_Read_Word(IO_EXTENSION.addr, IO_EXTENSION_RTC_INT_ADDR);
}
