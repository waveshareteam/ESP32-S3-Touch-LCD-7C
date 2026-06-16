/*****************************************************************************
 * | File      	 :   main.c
 * | Author      :   Waveshare team
 * | Function    :   Main function
 * | Info        :
 *                   LCD screen test, including drawing dots, 
 *                   lines, rectangles, circles, characters and pictures
 *----------------
 * |This version :   V1.0
 * | Date        :   2024-11-19
 * | Info        :   Basic version
 *
 ******************************************************************************/
#include "rgb_lcd_port.h"    // Header for Waveshare RGB LCD driver
#include "gui_paint.h"       // Header for graphical drawing functions
#include "image.h"           // Header for image resources

#define ROTATE ROTATE_0//rotate = 0, 90, 180, 270

void app_main()
{
    // Initialize I2C communication and CH422G hardware interface
    DEV_I2C_Init();
    IO_EXTENSION_Init();
    
    // 以下 IO 为输出，其余输入
    uint16_t init_mode =
      (1U << IO_EXTENSION_IO_1)
    | (1U << IO_EXTENSION_IO_2)
    | (1U << IO_EXTENSION_IO_3)
    | (1U << IO_EXTENSION_IO_4)
    | (1U << IO_EXTENSION_IO_6)
    | (1U << IO_EXTENSION_IO_11)
    | (1U << IO_EXTENSION_IO_12)
    | (1U << IO_EXTENSION_IO_13)
    | (1U << IO_EXTENSION_IO_14);

    IO_EXTENSION_SetAllMode(init_mode); // 设置 EXIO7 ~ EXIO10 为输入模式, EXIO11 ~ EXIO14 为输出模式

    // Initialize the Waveshare ESP32-S3 RGB LCD
    waveshare_esp32_s3_rgb_lcd_init(); 

    // Turn on the LCD backlight
    waveshare_rgb_lcd_bl_on();         
    // Uncomment the following line to turn off the backlight if needed
    // waveshare_rgb_lcd_bl_off();

    // Allocate memory for the screen's frame buffer
    UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2; // Each pixel takes 2 bytes in RGB565
    UBYTE *BlackImage;
    if ((BlackImage = (UBYTE *)malloc(Imagesize)) == NULL) // Allocate memory
    {
        printf("Failed to apply for black memory...\r\n");
        exit(0); // Exit the program if memory allocation fails
    }

    // Create a new image canvas and set its background color to white
    Paint_NewImage(BlackImage, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);

    // Set the canvas scale
    Paint_SetScale(65);
    Paint_SetRotate(ROTATE);
    // Clear the canvas and fill it with a white background
    Paint_Clear(WHITE);

    uint8_t io[4] = {0}, DI_flag = 0, num = 0;
    while (1)
    {
        IO_EXTENSION_Output(IO_EXTENSION_IO_11, 1);
        IO_EXTENSION_Output(IO_EXTENSION_IO_12, 0);
        IO_EXTENSION_Output(IO_EXTENSION_IO_13, 1);
        IO_EXTENSION_Output(IO_EXTENSION_IO_14, 0);

        vTaskDelay(5 / portTICK_PERIOD_MS);

        io[0] = IO_EXTENSION_Input(IO_EXTENSION_IO_7); 
        io[1] = IO_EXTENSION_Input(IO_EXTENSION_IO_8); 
        io[2] = IO_EXTENSION_Input(IO_EXTENSION_IO_9); 
        io[3] = IO_EXTENSION_Input(IO_EXTENSION_IO_10); 

        // Check if both pins match expected values
        if (io[0] == 1 && io[1] == 0 && io[2] == 1 && io[3] == 0)
        {
            DI_flag++; // Increment DI flag
        }

        IO_EXTENSION_Output(IO_EXTENSION_IO_11, 0);
        IO_EXTENSION_Output(IO_EXTENSION_IO_12, 1);
        IO_EXTENSION_Output(IO_EXTENSION_IO_13, 0);
        IO_EXTENSION_Output(IO_EXTENSION_IO_14, 1);

        vTaskDelay(5 / portTICK_PERIOD_MS);

        io[0] = IO_EXTENSION_Input(IO_EXTENSION_IO_7); 
        io[1] = IO_EXTENSION_Input(IO_EXTENSION_IO_8); 
        io[2] = IO_EXTENSION_Input(IO_EXTENSION_IO_9); 
        io[3] = IO_EXTENSION_Input(IO_EXTENSION_IO_10); 

        // Check again if both pins match expected values
        if (io[0] == 0 && io[1] == 1 && io[2] == 0 && io[3] == 1)
        {
            DI_flag++; // Increment DI flag
        }
        printf("DI_flag:%d\r\n",DI_flag);
        // If both conditions are met, DI & DO are working
        if (DI_flag >= 2)
        {
            printf("DI & DO OK!!!\r\n"); // DI and DO are functioning properly
            Paint_Clear(GREEN);
            // Display the gradient on the screen
            waveshare_rgb_lcd_display(BlackImage);
            break;
        }
        else
        {
            num++;        // Add 1 to the count
            DI_flag=0;
            if (num == 3) // If the test fails three times, we quit
            {
                printf("DI & DO Failure!!!\r\n"); // DI and DO are not functioning
                Paint_Clear(RED);
                // Display the gradient on the screen
                waveshare_rgb_lcd_display(BlackImage);
                break;
            }
        }
    }
    
}   
   
   