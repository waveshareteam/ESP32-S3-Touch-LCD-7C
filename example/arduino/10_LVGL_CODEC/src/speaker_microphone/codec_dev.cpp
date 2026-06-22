#include "codec_dev.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <Audio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "codec_dev";
static const char *audio_mount_point = "/sdcard";

static Audio audio_player(I2S_NUM_1);
static esp_codec_dev_handle_t play_dev_handle = NULL;
static bool codec_inited = false;
static bool player_inited = false;
static bool player_paused = false;
static int volume_intensity = CODEC_DEFAULT_VOLUME;
static int gain_intensity = CODEC_DEFAULT_ADC_VOLUME;
static volatile bool pending_eof = false;
static audio_player_cb_t player_callback = NULL;
static audio_player_callback_event_t player_state = AUDIO_PLAYER_CALLBACK_EVENT_IDLE;
static portMUX_TYPE player_cmd_mux = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t volume_commit_task_handle = NULL;
static char pending_play_path[512];
static bool pending_play_start_paused = false;
static volatile bool pending_volume_update = false;
static int pending_volume = CODEC_DEFAULT_VOLUME;

#define VOLUME_COMMIT_PERIOD_MS  (40)
#define VOLUME_COMMIT_TASK_STACK (3072)
#define VOLUME_COMMIT_TASK_PRIO  (1)

typedef enum {
    PLAYER_CMD_NONE = 0,
    PLAYER_CMD_PLAY,
    PLAYER_CMD_PAUSE,
    PLAYER_CMD_RESUME,
} player_cmd;

static volatile player_cmd pending_cmd = PLAYER_CMD_NONE;

static void set_player_state(audio_player_callback_event_t state, bool paused)
{
    portENTER_CRITICAL(&player_cmd_mux);
    player_state = state;
    player_paused = paused;
    portEXIT_CRITICAL(&player_cmd_mux);
}

static audio_player_callback_event_t get_player_state_locked(void)
{
    audio_player_callback_event_t state;

    portENTER_CRITICAL(&player_cmd_mux);
    state = player_state;
    portEXIT_CRITICAL(&player_cmd_mux);

    return state;
}

static bool get_player_paused_locked(void)
{
    bool paused;

    portENTER_CRITICAL(&player_cmd_mux);
    paused = player_paused;
    portEXIT_CRITICAL(&player_cmd_mux);

    return paused;
}

static void log_audio_message(Audio::msg_t msg)
{
    const char *msg_tag = msg.s ? msg.s : "audio";
    const char *text = msg.msg ? msg.msg : "";
    Serial.printf("[%s] %s\n", msg_tag, text);
}

static void audio_info_router(Audio::msg_t msg)
{
    log_audio_message(msg);
    if (msg.e == Audio::evt_eof) {
        pending_eof = true;
    }
}

static const char *normalize_audio_path(const char *file_path)
{
    if (file_path == NULL) {
        return NULL;
    }

    size_t prefix_len = strlen(audio_mount_point);
    if (strncmp(file_path, audio_mount_point, prefix_len) == 0) {
        return file_path + prefix_len;
    }

    return file_path;
}

static int clamp_output_volume(int volume)
{
    if (volume < 0) {
        return 0;
    }
    if (volume > 100) {
        return 100;
    }
    return volume;
}

static void apply_player_volume_software(int volume)
{
    int clamped = clamp_output_volume(volume);

    audio_player.setVolume(clamped);
    portENTER_CRITICAL(&player_cmd_mux);
    volume_intensity = clamped;
    portEXIT_CRITICAL(&player_cmd_mux);
}

static esp_err_t apply_player_volume_hardware(int volume, int *volume_set)
{
    int clamped = clamp_output_volume(volume);

    if (volume_set) {
        *volume_set = clamped;
    }

    if (play_dev_handle) {
        int ret = esp_codec_dev_set_out_vol(play_dev_handle, clamped);
        if (ret != ESP_CODEC_DEV_OK) {
            ESP_LOGW(TAG, "set codec volume failed: %d", ret);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

static void volume_commit_task(void *arg)
{
    while (1) {
        int volume = CODEC_DEFAULT_VOLUME;
        bool apply_volume = false;

        vTaskDelay(pdMS_TO_TICKS(VOLUME_COMMIT_PERIOD_MS));

        portENTER_CRITICAL(&player_cmd_mux);
        apply_volume = pending_volume_update;
        volume = pending_volume;
        pending_volume_update = false;
        portEXIT_CRITICAL(&player_cmd_mux);

        if (apply_volume) {
            (void) apply_player_volume_hardware(volume, NULL);
        }
    }
}

esp_err_t mic_i2s_read(void *audio_buffer, size_t len, size_t *bytes_read, uint32_t timeout_ms)
{
    if (bytes_read) {
        *bytes_read = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t speaker_i2s_write(void *audio_buffer, size_t len, size_t *bytes_written, uint32_t timeout_ms)
{
    if (bytes_written) {
        *bytes_written = 0;
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t speaker_codec_set_fs(uint32_t rate, uint32_t bits_cfg, i2s_slot_mode_t ch)
{
    return ESP_OK;
}

esp_err_t speaker_codec_mute_set(bool enable)
{
    audio_player.setMute(enable);
    if (play_dev_handle) {
        int ret = esp_codec_dev_set_out_mute(play_dev_handle, enable);
        return ret == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t speaker_codec_volume_set(int volume, int *volume_set)
{
    apply_player_volume_software(volume);
    return apply_player_volume_hardware(volume, volume_set);
}

esp_err_t microphone_codec_gain_set(int gain, int *gain_set)
{
    gain_intensity = gain;
    if (gain_set) {
        *gain_set = gain;
    }
    return ESP_OK;
}

int speaker_codec_volume_get(void)
{
    int volume;

    portENTER_CRITICAL(&player_cmd_mux);
    volume = volume_intensity;
    portEXIT_CRITICAL(&player_cmd_mux);

    return volume;
}

esp_err_t speaker_codec_dev_stop(void)
{
    audio_player.stopSong();
    set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_IDLE, false);
    return ESP_OK;
}

esp_err_t speaker_codec_dev_resume(void)
{
    return get_player_paused_locked() ? audio_player_resume() : ESP_OK;
}

esp_err_t codec_init(void)
{
    if (codec_inited) {
        return ESP_OK;
    }

    play_dev_handle = speaker_init();
    ESP_RETURN_ON_FALSE(play_dev_handle, ESP_FAIL, TAG, "speaker_init failed");

    esp_codec_dev_sample_info_t fs = {};
    fs.sample_rate = CODEC_DEFAULT_SAMPLE_RATE;
    fs.channel = I2S_SLOT_MODE_STEREO;
    fs.bits_per_sample = CODEC_DEFAULT_BIT_WIDTH;

    int open_ret = esp_codec_dev_open(play_dev_handle, &fs);
    ESP_RETURN_ON_FALSE(open_ret == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "esp_codec_dev_open failed: %d", open_ret);

    int mute_ret = esp_codec_dev_set_out_mute(play_dev_handle, false);
    if (mute_ret != ESP_CODEC_DEV_OK) {
        ESP_LOGW(TAG, "set codec unmute failed: %d", mute_ret);
    }

    ESP_ERROR_CHECK(speaker_i2s_release());

    codec_inited = true;
    return speaker_codec_volume_set(CODEC_DEFAULT_VOLUME, NULL);
}

esp_err_t speaker_player_init(void)
{
    if (player_inited) {
        return ESP_OK;
    }

    Audio::audio_info_callback = audio_info_router;
    bool pinout_ok = audio_player.setPinout(I2S_SCLK, I2S_LCLK, I2S_DOUT, I2S_MCLK);
    ESP_RETURN_ON_FALSE(pinout_ok, ESP_FAIL, TAG, "audio setPinout failed");
    audio_player.setOutput48KHz(true);
    audio_player.setVolumeSteps(100);
    audio_player.setVolume(CODEC_DEFAULT_VOLUME);
    audio_player.setMute(false);

    if (volume_commit_task_handle == NULL) {
        BaseType_t ret = xTaskCreatePinnedToCore(volume_commit_task,
                                                 "vol_commit",
                                                 VOLUME_COMMIT_TASK_STACK,
                                                 NULL,
                                                 VOLUME_COMMIT_TASK_PRIO,
                                                 &volume_commit_task_handle,
                                                 tskNO_AFFINITY);
        ESP_RETURN_ON_FALSE(ret == pdPASS, ESP_FAIL, TAG, "volume task create failed");
    }

    set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_IDLE, false);
    player_inited = true;
    return ESP_OK;
}

void speaker_player_process(void)
{
    player_cmd cmd = PLAYER_CMD_NONE;
    bool start_paused = false;
    char play_path[sizeof(pending_play_path)] = {0};

    if (!player_inited) {
        return;
    }

    portENTER_CRITICAL(&player_cmd_mux);
    cmd = pending_cmd;
    if (cmd == PLAYER_CMD_PLAY) {
        start_paused = pending_play_start_paused;
        strncpy(play_path, pending_play_path, sizeof(play_path) - 1);
    }
    pending_cmd = PLAYER_CMD_NONE;
    pending_play_start_paused = false;
    pending_play_path[0] = '\0';
    portEXIT_CRITICAL(&player_cmd_mux);

    switch (cmd) {
        case PLAYER_CMD_PLAY:
            if (play_path[0] != '\0' && speaker_player_play_file(play_path) == ESP_OK && start_paused) {
                audio_player_pause();
            }
            break;
        case PLAYER_CMD_PAUSE:
            audio_player_pause();
            break;
        case PLAYER_CMD_RESUME:
            audio_player_resume();
            break;
        default:
            break;
    }

    audio_player.loop();

    if (pending_eof) {
        pending_eof = false;
        set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_IDLE, false);
        if (player_callback) {
            audio_player_cb_ctx_t ctx = {
                .audio_event = AUDIO_PLAYER_CALLBACK_EVENT_IDLE,
                .file_path = NULL,
            };
            player_callback(&ctx);
        }
    }
}

esp_err_t speaker_player_play_file(const char *file_path)
{
    const char *audio_path = normalize_audio_path(file_path);
    ESP_RETURN_ON_FALSE(audio_path, ESP_ERR_INVALID_ARG, TAG, "invalid file path");
    ESP_RETURN_ON_FALSE(player_inited, ESP_ERR_INVALID_STATE, TAG, "speaker player not initialized");

    pending_eof = false;
    set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_IDLE, false);
    audio_player.stopSong();
    Serial.printf("[audio] connecttoFS: %s\n", audio_path);

    bool ok = audio_player.connecttoFS(SD_MMC, audio_path);
    ESP_RETURN_ON_FALSE(ok, ESP_FAIL, TAG, "connecttoFS failed: %s", audio_path);
    set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_PLAYING, false);
    return ESP_OK;
}

esp_err_t speaker_player_request_play_file(const char *file_path, bool start_paused)
{
    ESP_RETURN_ON_FALSE(file_path, ESP_ERR_INVALID_ARG, TAG, "invalid file path");
    ESP_RETURN_ON_FALSE(player_inited, ESP_ERR_INVALID_STATE, TAG, "speaker player not initialized");

    portENTER_CRITICAL(&player_cmd_mux);
    strncpy(pending_play_path, file_path, sizeof(pending_play_path) - 1);
    pending_play_path[sizeof(pending_play_path) - 1] = '\0';
    pending_play_start_paused = start_paused;
    pending_cmd = PLAYER_CMD_PLAY;
    portEXIT_CRITICAL(&player_cmd_mux);

    return ESP_OK;
}

esp_err_t speaker_player_request_pause(void)
{
    ESP_RETURN_ON_FALSE(player_inited, ESP_ERR_INVALID_STATE, TAG, "speaker player not initialized");

    portENTER_CRITICAL(&player_cmd_mux);
    pending_cmd = PLAYER_CMD_PAUSE;
    portEXIT_CRITICAL(&player_cmd_mux);

    return ESP_OK;
}

esp_err_t speaker_player_request_resume(void)
{
    ESP_RETURN_ON_FALSE(player_inited, ESP_ERR_INVALID_STATE, TAG, "speaker player not initialized");

    portENTER_CRITICAL(&player_cmd_mux);
    pending_cmd = PLAYER_CMD_RESUME;
    portEXIT_CRITICAL(&player_cmd_mux);

    return ESP_OK;
}

esp_err_t speaker_player_request_volume(int volume)
{
    int clamped = clamp_output_volume(volume);

    ESP_RETURN_ON_FALSE(player_inited, ESP_ERR_INVALID_STATE, TAG, "speaker player not initialized");

    apply_player_volume_software(clamped);

    portENTER_CRITICAL(&player_cmd_mux);
    pending_volume = clamped;
    pending_volume_update = true;
    portEXIT_CRITICAL(&player_cmd_mux);

    return ESP_OK;
}

void speaker_player_register_callback(audio_player_cb_t cb, void *user_data)
{
    (void) user_data;
    player_callback = cb;
}

esp_err_t audio_player_pause(void)
{
    if (!player_inited || get_player_paused_locked() || !audio_player.isRunning()) {
        return ESP_OK;
    }

    audio_player.pauseResume();
    set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_PAUSE, true);
    return ESP_OK;
}

esp_err_t audio_player_resume(void)
{
    if (!player_inited || !get_player_paused_locked()) {
        return ESP_OK;
    }

    audio_player.pauseResume();
    set_player_state(AUDIO_PLAYER_CALLBACK_EVENT_PLAYING, false);
    return ESP_OK;
}

bool speaker_player_is_paused(void)
{
    return get_player_paused_locked();
}

audio_player_callback_event_t speaker_player_get_state(void)
{
    return get_player_state_locked();
}
