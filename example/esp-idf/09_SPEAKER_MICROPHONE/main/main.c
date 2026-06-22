/*****************************************************************************
 * | File       :   main.c
 * | Author     :   Waveshare team
 * | Function   :   Main function
 * | Info       :   Record and play audio using I2S with LCD and touch support
 * | Version    :   V1.0
 * | Date       :   2024-12-06
 * | Language   :   C (ESP-IDF)
 ******************************************************************************/

#include "rgb_lcd_port.h" // LCD display driver
#include "gui_paint.h"    // GUI drawing functions
#include "gt911.h"        // GT911 touch controller
#include "sd.h"           // SD card (not used directly here)
#include "codec_dev.h"    // Codec driver
#include "format_wav.h"   // WAV formatting (not used directly here)
#include "esp_check.h"    // Error handling macros

static const char *TAG = "main";

// Configuration macros
#define RECORD_TIME_SEC 5
#define BUFFER_SIZE (CODEC_DEFAULT_SAMPLE_RATE * RECORD_TIME_SEC * CODEC_DEFAULT_CHANNEL * (CODEC_DEFAULT_BIT_WIDTH / 8))

static int16_t *record_buffer = NULL;
UBYTE *BlackImage;

// Function to handle recording and playback
void play_or_pause(bool play)
{
    if (play)
    {
        // Recording logic
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
                         &bytes_read, portMAX_DELAY);
            total_bytes += bytes_read;
        }

        ESP_LOGI(TAG, "Recording done.");
        Paint_Clear(WHITE);
        waveshare_rgb_lcd_display(BlackImage);
    }
    else
    {
        // Playback logic
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
                              &bytes_written, portMAX_DELAY);
            total_bytes += bytes_written;
        }
        speaker_codec_mute_set(true);
    }
}

#define AUDIO_FRAME_SIZE 1024   // 每次读写样本数，可调
static int16_t audio_frame[AUDIO_FRAME_SIZE * 4]; // 4通道 * 4

void app_main()
{
    touch_gt911_point_t point_data;
    DEV_I2C_Init();
    IO_EXTENSION_Init();
    touch_gt911_init(DEV_I2C_Get_Bus_Device());
    waveshare_esp32_s3_rgb_lcd_init();
    waveshare_rgb_lcd_bl_on();
    IO_EXTENSION_Pwm_Output(100);
    // Allocate LCD frame buffer
    UDOUBLE Imagesize = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * 2;
    BlackImage = (UBYTE *)malloc(Imagesize);
    if (!BlackImage)
    {
        printf("Failed to allocate memory for frame buffer...\r\n");
        exit(0);
    }

    Paint_NewImage(BlackImage, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES, 0, WHITE);
    Paint_SetScale(65);
    Paint_Clear(WHITE);

    // Draw initial red record button
    Paint_DrawCircle(405, 450, 15, RED, DOT_PIXEL_2X2, DRAW_FILL_FULL);
    Paint_DrawString_EN(100, 150, "Click to record/play", &Font48, BLACK, WHITE);
    waveshare_rgb_lcd_display(BlackImage);

    // Initialize speaker codec
    codec_init();
    speaker_codec_volume_set(100, NULL);
    microphone_codec_gain_set(30, NULL);

    // Allocate memory for recording buffer
    record_buffer = heap_caps_malloc(BUFFER_SIZE * sizeof(int16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!record_buffer)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer");
        vTaskDelete(NULL);
        return;
    }

    // Touch handling loop
    static uint16_t prev_x;
    static uint16_t prev_y;
    bool is_playing = false;
    // bool realtime_play = false;
    while (1)
    {
        // point_data = touch_gt911_read_point(1);
        // if (point_data.cnt == 1)
        // {
        //     if (prev_x == point_data.x[0] && prev_y == point_data.y[0])
        //     {
        //         continue;
        //     }
        //     else if (point_data.x[0] > 390 && point_data.x[0] < 420 &&
        //              point_data.y[0] > 420 && point_data.y[0] < 480)
        //     {
        //         Paint_Clear(WHITE);
        //         is_playing = !is_playing;
        //         play_or_pause(is_playing);

        //         prev_x = point_data.x[0];
        //         prev_y = point_data.y[0];
        //     }
        // }
        // vTaskDelay(30 / portTICK_PERIOD_MS);
    


        point_data = touch_gt911_read_point(1);
        if (point_data.cnt == 1)
        {
            if (!(prev_x == point_data.x[0] && prev_y == point_data.y[0]))
            {
                if (point_data.x[0] > 390 && point_data.x[0] < 420 &&
                    point_data.y[0] > 420 && point_data.y[0] < 480)
                {
                    Paint_Clear(WHITE);
                    is_playing = true;
                    play_or_pause(is_playing);
                    is_playing = false;
                    play_or_pause(is_playing);

                    Paint_Clear(WHITE);
                    Paint_DrawCircle(405, 450, 15, RED, DOT_PIXEL_2X2, DRAW_FILL_FULL);
                    Paint_DrawString_EN(100, 150, "Click to record/play", &Font48, BLACK, WHITE);
                    waveshare_rgb_lcd_display(BlackImage);

                    prev_x = point_data.x[0];
                    prev_y = point_data.y[0];
                }
            }
        }

        // Realtime test path, preserved for debug only.
        // if (realtime_play)
        // {
        //     size_t bytes_read = 0;
        //     size_t bytes_written = 0;

        //     // 从麦克风读取
        //     mic_i2s_read(audio_frame, AUDIO_FRAME_SIZE * sizeof(int16_t), &bytes_read, portMAX_DELAY);

        //     // 马上播放到扬声器
        //     speaker_i2s_write(audio_frame, bytes_read, &bytes_written, portMAX_DELAY);
        // }

        // if (realtime_play)
        // {
        //     size_t bytes_read = 0;
        //     size_t bytes_written = 0;
        //
        //     // 读取 4 通道数据
        //     mic_i2s_read(audio_frame,
        //                 AUDIO_FRAME_SIZE * 4 * sizeof(int16_t),
        //                 &bytes_read,
        //                 portMAX_DELAY);
        //
        //     int16_t *in  = (int16_t *)audio_frame;
        //
        //     // 4ch → 2ch 输出 buffer
        //     static int16_t out_frame[AUDIO_FRAME_SIZE * 2];
        //     int16_t *out = out_frame;
        //
        //     int total_samples = bytes_read / sizeof(int16_t);
        //
        //     // 每 4 个样本是一帧
        //     int frame_count = total_samples / 4;
        //
        //     for (int i = 0; i < frame_count; i++)
        //     {
        //         out[i * 2 + 0] = in[i * 4 + 3]; // CH0
        //         out[i * 2 + 1] = in[i * 4 + 0]; // CH3
        //         //0是MIC1  1是MIC3 2是MIC2 3是MIC4
        //     }
        //
        //     static int16_t audio[AUDIO_FRAME_SIZE * 2];
        //     for (int i = 0; i < AUDIO_FRAME_SIZE; i++) {
        //         audio[i * 2 + 0] = 20000 * sin(2 * M_PI * 1000 * i / 16000);
        //         audio[i * 2 + 1] = 20000 * sin(2 * M_PI * 1000 * i / 16000);
        //     }
        //
        //     // 写 2 通道数据
        //     speaker_i2s_write(audio,
        //                       AUDIO_FRAME_SIZE * 2 * sizeof(int16_t),
        //                       &bytes_written,
        //                       portMAX_DELAY);
        // }
        vTaskDelay(20 / portTICK_PERIOD_MS); // GUI刷新间隔，不影响音频
    }
}
