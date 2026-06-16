/*****************************************************************************
 * | File       :   main.c
 * | Author     :   Waveshare team
 * | Function   :   Main function
 * | Info       :   Record and play audio using I2S with LCD and touch support
 * | Version    :   V1.0
 * | Date       :   2024-12-06
 * | Language   :   C (ESP-IDF)
 ******************************************************************************/

#include "src/rgb_lcd_port/rgb_lcd_port.h"
#include "src/rgb_lcd_port/gui_paint/gui_paint.h"
#include "src/touch/gt911.h"
#include "src/speaker_microphone/codec_dev.h"

static const char *TAG = "main";

#define RECORD_TIME_SEC 5
#define BUFFER_SIZE (CODEC_DEFAULT_SAMPLE_RATE * RECORD_TIME_SEC * CODEC_DEFAULT_CHANNEL * (CODEC_DEFAULT_BIT_WIDTH / 8))

static int16_t *record_buffer = NULL;
static UBYTE *BlackImage = NULL;
static uint16_t prev_x;
static uint16_t prev_y;

static void draw_idle_screen()
{
    Paint_Clear(WHITE);
    Paint_DrawCircle(405, 450, 15, RED, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    Paint_DrawString_EN(100, 150, "Click to record/play", &Font48, BLACK, WHITE);
    waveshare_rgb_lcd_display(BlackImage);
}

static bool is_record_button_pressed(const touch_gt911_point_t *point)
{
    if (point->x[0] <= 390 || point->x[0] >= 420)
    {
        return false;
    }

    if (point->y[0] <= 420 || point->y[0] >= 480)
    {
        return false;
    }

    return true;
}

void play_or_pause(bool play)
{
    if (play)
    {
        speaker_codec_mute_set(true);
        Paint_DrawLine(390, 435, 390, 465, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawLine(410, 435, 410, 465, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
        Paint_DrawString_EN(200, 150, "Start recording...", &Font48, BLACK, WHITE);
        waveshare_rgb_lcd_display(BlackImage);
        ESP_LOGI(TAG, "Start recording...");

        size_t total_bytes = 0;
        while (total_bytes < BUFFER_SIZE)
        {
            size_t bytes_read = 0;
            mic_i2s_read(record_buffer + total_bytes / 2,
                         BUFFER_SIZE - total_bytes,
                         &bytes_read,
                         portMAX_DELAY);
            total_bytes += bytes_read;
        }

        ESP_LOGI(TAG, "Recording done.");
        Paint_Clear(WHITE);
        waveshare_rgb_lcd_display(BlackImage);
        return;
    }

    speaker_codec_mute_set(false);
    Paint_DrawLine(390, 435, 390, 465, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawLine(410, 435, 410, 465, RED, DOT_PIXEL_2X2, LINE_STYLE_SOLID);
    Paint_DrawString_EN(200, 150, "Start playing...", &Font48, BLACK, WHITE);
    waveshare_rgb_lcd_display(BlackImage);
    ESP_LOGI(TAG, "Start playing...");

    size_t total_bytes = 0;
    while (total_bytes < BUFFER_SIZE)
    {
        size_t bytes_written = 0;
        speaker_i2s_write(record_buffer + total_bytes / 2,
                          BUFFER_SIZE - total_bytes,
                          &bytes_written,
                          portMAX_DELAY);
        total_bytes += bytes_written;
    }
    speaker_codec_mute_set(true);
}

void setup()
{
    DEV_I2C_Init();
    IO_EXTENSION_Init();
    touch_gt911_init(DEV_I2C_Get_Bus_Device());
    waveshare_esp32_s3_rgb_lcd_init();
    waveshare_rgb_lcd_bl_on();
    IO_EXTENSION_Pwm_Output(100);

    UDOUBLE image_size = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2;
    BlackImage = (UBYTE *)heap_caps_malloc(image_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (BlackImage == NULL)
    {
        printf("Failed to allocate memory for frame buffer...\r\n");
        while (true)
        {
            delay(1000);
        }
    }

    Paint_NewImage(BlackImage, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);
    Paint_SetScale(65);
    draw_idle_screen();

    codec_init();
    speaker_codec_volume_set(100, NULL);
    microphone_codec_gain_set(30, NULL);

    record_buffer = (int16_t *)heap_caps_malloc(BUFFER_SIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (record_buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        while (true)
        {
            delay(1000);
        }
    }
}

void loop()
{
    touch_gt911_point_t point_data = touch_gt911_read_point(1);

    if (point_data.cnt != 1)
    {
        delay(20);
        return;
    }

    if (prev_x == point_data.x[0] && prev_y == point_data.y[0])
    {
        delay(20);
        return;
    }

    if (is_record_button_pressed(&point_data))
    {
        Paint_Clear(WHITE);
        play_or_pause(true);
        play_or_pause(false);
        draw_idle_screen();
    }

    prev_x = point_data.x[0];
    prev_y = point_data.y[0];
    delay(20);
}
