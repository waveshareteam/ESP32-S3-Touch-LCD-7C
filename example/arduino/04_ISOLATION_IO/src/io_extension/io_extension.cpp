/*****************************************************************************
 * | File         :   io_extension.c
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
#include "io_extension.h"  // Include IO_EXTENSION driver header for GPIO functions
 
io_extension_obj_t IO_EXTENSION;  // Define the global IO_EXTENSION object

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
    if (pin > 15) {
        return;
    }

    if (mode) {
        IO_EXTENSION.Mode_value |= (1U << pin);
    } else {
        IO_EXTENSION.Mode_value &= ~(1U << pin);
    }

    IO_EXTENSION_SetAllMode(IO_EXTENSION.Mode_value);
}

/**
 * @brief Initialize the IO_EXTENSION device.
 * 
 * This function configures the slave addresses for different registers of the
 * IO_EXTENSION chip via I2C, and sets the control flags for input/output modes.
 */
void IO_EXTENSION_Init()
{
    // Set the I2C slave address for the IO_EXTENSION device
    DEV_I2C_Set_Slave_Addr(&IO_EXTENSION.addr, IO_EXTENSION_ADDR);

    IO_EXTENSION_SetAllMode(0xFFFF); // Set all pins to output mode

    // Initialize control flags for IO output enable and open-drain output mode
    IO_EXTENSION.Last_io_value = 0xFFFF; // All pins are initially set to high
}

/**
 * @brief Set the value of the IO output pins on the IO_EXTENSION device.
 * 
 * This function writes an 8-bit value to the IO output register. The value
 * determines the high or low state of the pins.
 * 
 * @param pin The pin number to set (0-7).
 * @param value The value to set on the specified pin (0 = low, 1 = high).
 */
void IO_EXTENSION_Output(uint8_t pin, uint8_t value)
{
    if (pin > 15) {
        return;
    }

    if (value) {
        IO_EXTENSION.Last_io_value |= (1U << pin);
    } else {
        IO_EXTENSION.Last_io_value &= ~(1U << pin);
    }

    uint8_t data[3];
    data[0] = IO_EXTENSION_IO_OUTPUT_ADDR;
    data[1] = (uint8_t)(IO_EXTENSION.Last_io_value & 0xFF);
    data[2] = (uint8_t)((IO_EXTENSION.Last_io_value >> 8) & 0xFF);
    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 3);
}

/**
 * @brief Read the value from the IO input pins on the IO_EXTENSION device.
 * 
 * This function reads the value of the IO input register and returns the state
 * of the specified pins.
 * 
 * @param pin The bit mask to specify which pin to read (e.g., 0x01 for the first pin).
 * @return The value of the specified pin(s) (0 = low, 1 = high).
 */
uint8_t IO_EXTENSION_Input(uint8_t pin) 
{
    uint8_t buf[2] = {0};
    uint16_t value = 0;

    if (pin > 15) {
        return 0;
    }

    DEV_I2C_Read_Nbyte(IO_EXTENSION.addr, IO_EXTENSION_IO_INPUT_ADDR, buf, 2);
    value = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    return (value & (1U << pin)) ? 1 : 0;
}

/**
 * @brief Set the PWM output value on the IO_EXTENSION device.
 * 
 * This function sets the PWM output value, which controls the duty cycle of the PWM signal.
 * The duty cycle is calculated based on the input value and the resolution (12 bits).
 * 
 * @param Value The input value to set the PWM duty cycle (0-100).
 */
void IO_EXTENSION_Pwm_Output(uint8_t Value)
{
    // Prevent the screen from completely turning off
    if (Value >= 95)
    {
        Value = 95;
    }

    uint8_t data[2] = {IO_EXTENSION_PWM_ADDR, Value}; // Prepare the data to write to the PWM register
    // Calculate the duty cycle based on the resolution (12 bits)
    data[1] = Value * (255 / 100.0);
    // Write the 8-bit value to the PWM output register
    DEV_I2C_Write_Nbyte(IO_EXTENSION.addr, data, 2);
}

/**
 * @brief Read the ADC input value from the IO_EXTENSION device.
 * 
 * This function reads the ADC input value from the IO_EXTENSION device.
 * 
 * @return The ADC input value.
 */
uint16_t IO_EXTENSION_Adc_Input()
{
    // Read the ADC input value from the IO_EXTENSION device
    return DEV_I2C_Read_Word(IO_EXTENSION.addr, IO_EXTENSION_ADC_ADDR);
}

/**
 * @brief  Read the status of the RTC interrupt pin
 * 
 * This function reads the RTC interrupt (INT) value 
 * from the IO_EXTENSION device via I2C.
 * 
 * @return The RTC INT value.
 */
uint8_t IO_EXTENSION_RTC_INT_READ()
{
    return DEV_I2C_Read_Word(IO_EXTENSION.addr, IO_EXTENSION_RTC_INT_ADDR);
}
