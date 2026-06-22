#include <Arduino.h>
#include <SD_MMC.h>
#include "src/i2c/i2c.h"
#include "src/io_extension/io_extension.h"
#include "src/rgb_lcd_port/rgb_lcd_port.h"
#include "src/touch/gt911.h"
#include "src/lvgl_port/lvgl_port.h"
#include "src/app_lvgl/Music_Controller.h"
#include "src/app_lvgl/Muisc_Player_UI.h"
#include "src/speaker_microphone/codec_dev.h"

#define MOUNT_POINT "/sdcard"                 // Mount point for SD card
#define MUSIC_DIR "/music"                    // File-system path used by SD_MMC.open()
#define EXAMPLE_FORMAT_IF_MOUNT_FAILED false  // Format SD card if mounting fails
#define EXAMPLE_PIN_CLK GPIO_NUM_12           // GPIO pin for SD card clock
#define EXAMPLE_PIN_CMD GPIO_NUM_11           // GPIO pin for SD card command line
#define EXAMPLE_PIN_D0 GPIO_NUM_13            // GPIO pin for SD card data line (D0)

static bool player_started = false;
static esp_lcd_panel_handle_t lcd_panel_handle = NULL;
static esp_lcd_touch_handle_t touch_panel_handle = NULL;
static volatile bool pending_auto_play_next = false;

static void audio_player_event_cb(audio_player_cb_ctx_t *ctx) {
  if (ctx == NULL) {
    return; 
  }

  if (ctx->audio_event == AUDIO_PLAYER_CALLBACK_EVENT_IDLE) {
    pending_auto_play_next = true;
  }
}

static void app_player_ui_sync(void) {
  const char *file_path = player_started ? get_file_path() : NULL;
  music_player_ui_set_audio_snapshot(
    speaker_player_get_state(),
    speaker_player_is_paused(),
    get_volume(),
    file_path);
}

static void player_ui_setup(void) {
  lcd_panel_handle = waveshare_esp32_s3_rgb_lcd_init();
  waveshare_rgb_lcd_bl_on();

  touch_panel_handle = touch_gt911_init(DEV_I2C_Get_Bus_Device());
  if (touch_panel_handle == NULL) {
    Serial.printf("touch_gt911_init failed\n");
  }

  if (lvgl_port_init(lcd_panel_handle, touch_panel_handle) != ESP_OK) {
    Serial.printf("lvgl_port_init failed\n");
    return;
  }

  if (lvgl_port_lock(-1)) {
    music_player_ui_init();
    lvgl_port_unlock();
  }
}

static bool sd_card_init(void) {
  SD_MMC.setPins(EXAMPLE_PIN_CLK, EXAMPLE_PIN_CMD, EXAMPLE_PIN_D0);
  return SD_MMC.begin(MOUNT_POINT, true);
}

static void player_audio_setup(void) {
  const char *initial_path = NULL;

  if (!sd_card_init()) {
    Serial.printf("SD_MMC begin failed\n");
    return;
  }

  if (!scan_music_dir(MUSIC_DIR)) {
    Serial.printf("No audio file found in %s\n", MUSIC_DIR);
    return;
  }

  if (codec_init() != ESP_OK) {
    Serial.printf("codec_init failed\n");
    return;
  }

  if (speaker_player_init() != ESP_OK) {
    Serial.printf("speaker_player_init failed\n");
    return;
  }

  speaker_player_register_callback(audio_player_event_cb, NULL);

  initial_path = get_file_path();
  if (initial_path == NULL) {
    Serial.printf("No initial track selected\n");
    return;
  }

  speaker_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL);
  if (speaker_player_play_file(initial_path) != ESP_OK) {
    Serial.printf("speaker_player_play_file failed: %s\n", initial_path);
    return;
  }

  Serial.printf("[audio] start: %s\n", initial_path);
  player_started = true;
}

void setup(void) {
  Serial.begin(115200);
  delay(100);
  Serial.println("[build] reusable_lvgl_player_ui_v1");
  DEV_I2C_Init();
  IO_EXTENSION_Init();
  player_ui_setup();
  player_audio_setup();
  app_player_ui_sync();
}

void loop(void) {
  int pending_volume = 0;

  speaker_player_process();

  if (music_player_ui_take_pending_volume(&pending_volume)) {
    (void)set_volume_sta(pending_volume);
  }

  if (pending_auto_play_next) {
    pending_auto_play_next = false;
    if (!play_next_music()) {
      Serial.printf("[audio] auto next failed\n");
    }
  }

  app_player_ui_sync();
  delay(10);
}
