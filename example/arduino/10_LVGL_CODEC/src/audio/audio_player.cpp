#include "audio_player.h"
#include <Audio.h>
#include "esp_log.h"
#include "Device_Decode/es8389.h"

static const char *tag = "audio_player";
static Audio audio;
static bool audio_ready = false;
static bool audio_paused = false;
static uint8_t audio_volume = 0;
static fs::FS *audio_file_system = NULL;

bool audio_player_init(i2c_master_bus_handle_t bus_handle,
                       fs::FS &file_system,
                       const char *file_path,
                       uint8_t volume)
{
    if (audio_ready) {
        return true;
    }

    if (file_path == NULL) {
        ESP_LOGE(tag, "file path is null");
        return false;
    }

    if (es8389_audio_init(bus_handle) != ESP_OK) {
        ESP_LOGE(tag, "ES8389 init failed");
        return false;
    }

    audio.setPinout(ES8389_I2S_BCLK_PIN,
                    ES8389_I2S_LRCK_PIN,
                    ES8389_I2S_DOUT_PIN,
                    ES8389_I2S_MCLK_PIN);
    audio.setVolume(volume);
    audio_volume = volume;
    audio_paused = false;
    audio_file_system = &file_system;

    audio_ready = true;
    return audio_player_play(file_path);
}

void audio_player_loop(void)
{
    if (audio_ready) {
        audio.loop();
    }
}

bool audio_player_play(const char *file_path)
{
    if (!audio_ready || audio_file_system == NULL || file_path == NULL) {
        return false;
    }

    audio.stopSong();
    if (!audio.connecttoFS(*audio_file_system, file_path)) {
        ESP_LOGE(tag, "open file failed: %s", file_path);
        return false;
    }

    audio_paused = false;
    ESP_LOGI(tag, "playback started: %s", file_path);
    return true;
}

bool audio_player_toggle_pause(void)
{
    if (!audio_ready) {
        return false;
    }

    audio.pauseResume();
    audio_paused = !audio_paused;
    ESP_LOGI(tag, audio_paused ? "playback paused" : "playback resumed");
    return true;
}

bool audio_player_is_paused(void)
{
    return audio_paused;
}

bool audio_player_set_volume(uint8_t volume)
{
    if (!audio_ready) {
        return false;
    }

    audio.setVolume(volume);
    audio_volume = volume;
    return true;
}

uint8_t audio_player_get_volume(void)
{
    return audio_volume;
}

