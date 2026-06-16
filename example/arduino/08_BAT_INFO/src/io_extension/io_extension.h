/*****************************************************************************
 * | File         :   io_extension.h
 * | Author       :   Waveshare team
 * | Function     :   GPIO control using io extension via I2C interface
 * | Info         :
 * |                 Header file for controlling GPIO pins via the io extension
 * |                 chip using I2C communication. This file defines the
 * |                 necessary I2C addresses, commands, and GPIO pin control
 * |                 functions.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2024-11-19
 * | Info         :   Basic version
 *
 ******************************************************************************/

 #ifndef __IO_EXTENSION_H
 #define __IO_EXTENSION_H

 #ifdef __cplusplus
 extern "C" {
 #endif
 
 #include "../i2c/i2c.h"  // Include I2C header for I2C communication functions
 
 /* 
  * IO EXTENSION GPIO control via I2C - Register and Command Definitions
  *
  *
  * Example usage:
  * 1. Set the working mode by writing to the register at address 0x24
  * 2. Send function commands to control the GPIO pins and modes
  */
 
 /* IO EXTENSION Function Register Addresses */
 #define IO_EXTENSION_ADDR          0x24  // Slave address for mode configuration register
 
 /* Mode control flags (from the chip manual) */
 #define IO_EXTENSION_Mode             0x02 // 
 #define IO_EXTENSION_IO_OUTPUT_ADDR   0x03 // 
 #define IO_EXTENSION_IO_INPUT_ADDR    0x04 // 
 #define IO_EXTENSION_PWM_ADDR         0x05 // 
 #define IO_EXTENSION_ADC_ADDR         0x06 // 
 #define IO_EXTENSION_RTC_INT_ADDR     0x07 // 
 
/* Specific IO pin bit masks */
#define IO_EXTENSION_IO_0     (1U << 0)   // IO0 
#define IO_EXTENSION_IO_1     (1U << 1)   // IO1 (used for touch reset)
#define IO_EXTENSION_IO_2     (1U << 2)   // IO2 (backlight control)
#define IO_EXTENSION_IO_3     (1U << 3)   // IO3 (PA)
#define IO_EXTENSION_IO_4     (1U << 4)   // IO4 (SD card CS pin)
#define IO_EXTENSION_IO_5     (1U << 5)   // IO5 (Select communication interface: 0 for USB, 1 for CAN)
#define IO_EXTENSION_IO_6     (1U << 6)   // IO6
#define IO_EXTENSION_IO_7     (1U << 7)   // IO7
#define IO_EXTENSION_IO_8     (1U << 8)   // IO8
#define IO_EXTENSION_IO_9     (1U << 9)   // IO9
#define IO_EXTENSION_IO_10    (1U << 10)  // IO10
#define IO_EXTENSION_IO_11    (1U << 11)  // IO11
#define IO_EXTENSION_IO_12    (1U << 12)  // IO12
#define IO_EXTENSION_IO_13    (1U << 13)  // IO13
#define IO_EXTENSION_IO_14    (1U << 14)  // IO14
#define IO_EXTENSION_IO_15    (1U << 15)  // IO15

 /* Structure to represent the IO EXTENSION device */
 typedef struct _io_extension_obj_t {
     DEV_I2C_DeviceHandle addr;      // Handle for mode configuration
     uint16_t Mode_value;
     uint16_t Last_io_value;
 } io_extension_obj_t;
 
 
 /* Function declarations */
 void IO_EXTENSION_SetAllMode(uint16_t mode_mask) ;     // Set all IO pin modes at once    
 void IO_EXTENSION_IO_Mode(uint8_t pin, uint8_t mode);      // Set IO pin modes (input/output)
 void IO_EXTENSION_Output(uint16_t mask, uint8_t value);     // Set IO pin output (high/low)
 uint8_t IO_EXTENSION_Input(uint8_t pin);   // Read IO pin input state
 void IO_EXTENSION_Pwm_Output(uint8_t Value);
 uint16_t IO_EXTENSION_Adc_Input();
 uint8_t IO_EXTENSION_RTC_INT_READ();
 void IO_EXTENSION_Init();                     // Initialize the IO_EXTENSION device

 #ifdef __cplusplus
 }
 #endif


 #endif  // __IO_EXTENSION_H
 
