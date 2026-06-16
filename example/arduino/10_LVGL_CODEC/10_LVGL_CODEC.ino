#include <Arduino.h>
#include <SD_MMC.h>
#include <string.h>
#include "src/audio/Config.h"
#include "src/audio/audio_player.h"
#include "src/i2c/i2c.h"
#include "src/io_extension/io_extension.h"
#include "src/rgb_lcd_port/rgb_lcd_port.h"
#include "src/touch/gt911.h"
#include "src/lvgl_port/lvgl_port.h"
#include "src/audio/UI/lyric_ui.h"
#include "src/audio/UI/player_ui.h"
#include "src/system/runtime_monitor.h"

typedef struct {
  i2c_master_bus_handle_t bus_handle;
  esp_lcd_panel_handle_t panel_handle;
  esp_lcd_touch_handle_t touch_handle;
} BoardCtx;

static BoardCtx board = {};
static bool audio_ready = false;
static uint32_t playback_elapsed_ms = 0;
static uint32_t playback_tick_ms = 0;
static const char *audio_track_list[] = {
  AUDIO_TRACK_PATH,
};
static const size_t audio_track_count = sizeof(audio_track_list) / sizeof(audio_track_list[0]);
static size_t current_track_index = 0;

bool switch_to_prev_track(void);
bool switch_to_next_track(void);
bool load_track_at_index(size_t track_index);

const char *get_current_track_path(void) {
  return audio_track_list[current_track_index];
}

void make_track_title(const char *track_path, char *title, size_t title_size) {
  if (track_path == NULL || title == NULL || title_size == 0) {
    return;
  }

  const char *base_name = strrchr(track_path, '/');
  base_name = (base_name == NULL) ? track_path : base_name + 1;

  snprintf(title, title_size, "%s", base_name);
  char *extension = strrchr(title, '.');
  if (extension != NULL) {
    *extension = '\0';
  }
}


void init_lrc(const char *track_path) {
  lyric_ui_init();

  if (!lyric_ui_load(SD_MMC, track_path)) {
    Serial.println("lyric load failed");
  }
}

BoardCtx init_board(void) {
  DEV_I2C_Init();
  IO_EXTENSION_Init();
  board.panel_handle = waveshare_esp32_s3_rgb_lcd_init();
  waveshare_rgb_lcd_bl_on();

  board.bus_handle = DEV_I2C_Get_Bus_Device();
  board.touch_handle = touch_gt911_init(board.bus_handle);

  return board;
}

void init_ui(void) {
  lvgl_port_init(board.panel_handle, board.touch_handle);
  player_ui_set_track_handlers(switch_to_prev_track, switch_to_next_track);
  player_ui_init();
}

bool init_sd(void) {
  SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN);
  return SD_MMC.begin(AUDIO_MOUNT_POINT, true);
}

bool init_audio(i2c_master_bus_handle_t bus_handle) {
  if (!init_sd()) {
    Serial.println("SD card init failed");
    return false;
  }

  if (!audio_player_init(bus_handle, SD_MMC, get_current_track_path(), AUDIO_PLAYER_VOLUME)) {
    Serial.println("player init failed");
    return false;
  }
  return true;
}

bool load_track_at_index(size_t track_index) {
  if (track_index >= audio_track_count) {
    return false;
  }

  current_track_index = track_index;
  const char *track_path = get_current_track_path();
  if (!audio_player_play(track_path)) {
    Serial.print("track play failed: ");
    Serial.println(track_path);
    return false;
  }

  if (!lyric_ui_load(SD_MMC, track_path)) {
    Serial.println("lyric load failed");
  }

  char track_title[64] = {0};
  make_track_title(track_path, track_title, sizeof(track_title));
  player_ui_set_track_title(track_title);
  player_ui_sync_state();

  playback_elapsed_ms = 0;
  playback_tick_ms = millis();
  Serial.print("[audio] playback started: ");
  Serial.println(track_path);
  return true;
}

bool switch_to_prev_track(void) {
  if (!audio_ready || audio_track_count == 0) {
    return false;
  }

  size_t prev_index = (current_track_index == 0) ? (audio_track_count - 1) : (current_track_index - 1);
  return load_track_at_index(prev_index);
}

bool switch_to_next_track(void) {
  if (!audio_ready || audio_track_count == 0) {
    return false;
  }

  size_t next_index = (current_track_index + 1) % audio_track_count;
  return load_track_at_index(next_index);
}

void setup(void) {
  Serial.begin(115200);
  init_board();
  runtime_monitor_start(5000);

  if (!init_audio(board.bus_handle)) {
    return;
  }

  init_ui();
  init_lrc(get_current_track_path());
  audio_ready = true;
  player_ui_sync_state();

  char track_title[64] = {0};
  make_track_title(get_current_track_path(), track_title, sizeof(track_title));
  player_ui_set_track_title(track_title);
  playback_elapsed_ms = 0;
  playback_tick_ms = millis();
  Serial.print("[audio] playback started: ");
  Serial.println(get_current_track_path());
}

void loop(void) {
  audio_player_loop();
  if (audio_ready) {
    uint32_t now_ms = millis();
    if (!audio_player_is_paused()) {
      playback_elapsed_ms += now_ms - playback_tick_ms;
    }
    playback_tick_ms = now_ms;
    lyric_ui_update(playback_elapsed_ms);
  }
  delay(10);
}

