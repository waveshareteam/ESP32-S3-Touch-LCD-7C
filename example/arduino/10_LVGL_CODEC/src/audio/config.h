#pragma once

#include "driver/gpio.h"

#define AUDIO_MOUNT_POINT "/sdcard"
#define AUDIO_TRACK_PATH "/music/BGM_1.mp3"

#define AUDIO_PLAYER_VOLUME 12

#define SD_CLK_PIN GPIO_NUM_12
#define SD_CMD_PIN GPIO_NUM_11
#define SD_D0_PIN GPIO_NUM_13

#define ES8389_I2S_BCLK_PIN GPIO_NUM_44
#define ES8389_I2S_MCLK_PIN GPIO_NUM_6
#define ES8389_I2S_LRCK_PIN GPIO_NUM_16
#define ES8389_I2S_DOUT_PIN GPIO_NUM_15
