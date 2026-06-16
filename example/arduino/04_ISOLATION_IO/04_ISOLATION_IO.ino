/*
* The LCD parameters and GPIO number used by this example can be changed in 
* [rgb_lcd_port.h](components/rgb_lcd_port.h). Especially, please pay attention 
* to the **vendor specific initialization**, it can be different between 
* manufacturers and should consult the LCD supplier for initialization sequence 
* code.
*/
#include "src/rgb_lcd_port/rgb_lcd_port.h"              // Header for Waveshare RGB LCD driver
#include "src/rgb_lcd_port/gui_paint/gui_paint.h"       // Header for graphical drawing functions
#include "src/rgb_lcd_port/image/image.h"               // Header for image resources

#define ROTATE ROTATE_0//rotate = 0, 90, 180, 270

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  DEV_I2C_Init();
  IO_EXTENSION_Init();

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
  IO_EXTENSION_SetAllMode(init_mode);

  // Initialize the Waveshare ESP32-S3 RGB LCD
  waveshare_esp32_s3_rgb_lcd_init(); 
  // Turn on the LCD backlight
  waveshare_rgb_lcd_bl_on();  

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
        if (io[0] == 1 && io[1] == 0 && io[2] == 1 && io[3] == 0)
        {
            DI_flag++;
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
        if (io[0] == 0 && io[1] == 1 && io[2] == 0 && io[3] == 1)
        {
            DI_flag++;
        }
        printf("DI_flag:%d\r\n",DI_flag);
        if (DI_flag >= 2)
        {
            printf("DI & DO OK!!!\r\n");
            Paint_Clear(GREEN);
            waveshare_rgb_lcd_display(BlackImage);
            break;
        }
        else
        {
            num++;
            DI_flag=0;
            if (num == 3)
            {
                printf("DI & DO Failure!!!\r\n");
                Paint_Clear(RED);
                waveshare_rgb_lcd_display(BlackImage);
                break;
            }
        }
    }
}

void loop() {
  // put your main code here, to run repeatedly:

}
