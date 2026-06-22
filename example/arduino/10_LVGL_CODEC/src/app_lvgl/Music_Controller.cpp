#include "Music_Controller.h"

#include <Arduino.h>
#include <SD_MMC.h>
#include <string.h>

#include "../speaker_microphone/codec_dev.h"

static const int app_max_tracks = 64;
static const int app_max_path_len = 256;
static const int app_min_codec_volume = 30;

static portMUX_TYPE player_bridge_mux = portMUX_INITIALIZER_UNLOCKED;
static char playlist_paths[app_max_tracks][app_max_path_len];
static int playlist_count = 0;
static int current_index = -1;
static int ui_volume = 100;
static bool ui_muted = false;

static int clamp_ui_volume(int volume)
{
    if (volume < 0) {
        return 0;
    }
    if (volume > 100) {
        return 100;
    }
    return volume;
}

static int map_ui_volume_to_codec(int volume)
{
    int clamped = clamp_ui_volume(volume);

    if (clamped <= 0) {
        return app_min_codec_volume;
    }

    return app_min_codec_volume + ((clamped * (100 - app_min_codec_volume)) / 100);
}

static bool is_mp3_file(const char *name)
{
    size_t len = strlen(name);
    return (len > 4) && (strcasecmp(name + len - 4, ".mp3") == 0);
}

static int clamp_track_index(int index)
{
    if (playlist_count <= 0) {
        return -1;
    }

    if (index < 0) {
        return playlist_count - 1;
    }

    if (index >= playlist_count) {
        return 0;
    }

    return index;
}

bool scan_music_dir(const char *base_path)
{
    File dir;
    int found_count = 0;

    if (base_path == NULL) {
        return false;
    }

    dir = SD_MMC.open(base_path);
    if (!dir || !dir.isDirectory()) {
        return false;
    }

    portENTER_CRITICAL(&player_bridge_mux);
    memset(playlist_paths, 0, sizeof(playlist_paths));
    playlist_count = 0;
    current_index = -1;
    portEXIT_CRITICAL(&player_bridge_mux);

    File entry = dir.openNextFile();
    while (entry && found_count < app_max_tracks) {
        if (!entry.isDirectory()) {
            const char *name = entry.name();
            if (name && is_mp3_file(name)) {
                const char *entry_path = entry.path();
                if (entry_path == NULL || entry_path[0] == '\0') {
                    entry_path = name;
                }

                portENTER_CRITICAL(&player_bridge_mux);
                strncpy(playlist_paths[found_count], entry_path, app_max_path_len - 1);
                playlist_paths[found_count][app_max_path_len - 1] = '\0';
                found_count++;
                playlist_count = found_count;
                portEXIT_CRITICAL(&player_bridge_mux);
            }
        }
        entry.close();
        entry = dir.openNextFile();
    }

    dir.close();

    if (found_count > 0) {
        portENTER_CRITICAL(&player_bridge_mux);
        current_index = 0;
        portEXIT_CRITICAL(&player_bridge_mux);
        return true;
    }

    return false;
}

const char *get_file_path(void)
{
    int index;
    int count;

    portENTER_CRITICAL(&player_bridge_mux);
    index = current_index;
    count = playlist_count;
    portEXIT_CRITICAL(&player_bridge_mux);

    if (index < 0 || index >= count) {
        return NULL;
    }

    return playlist_paths[index];
}

const char *get_file_name(const char *path)
{
    const char *name = path;

    if (path == NULL || path[0] == '\0') {
        return "(none)";
    }

    for (const char *p = path; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            name = p + 1;
        }
    }

    return (name[0] != '\0') ? name : path;
}

int get_track_count(void)
{
    int count;
    portENTER_CRITICAL(&player_bridge_mux);
    count = playlist_count;
    portEXIT_CRITICAL(&player_bridge_mux);
    return count;
}

int get_current_index(void)
{
    int index;
    portENTER_CRITICAL(&player_bridge_mux);
    index = current_index;
    portEXIT_CRITICAL(&player_bridge_mux);
    return index;
}

bool play_current(bool start_paused)
{
    const char *path = get_file_path();
    if (path == NULL) {
        return false;
    }

    return speaker_player_request_play_file(path, start_paused) == ESP_OK;
}

bool play_next_music(void)
{
    int index;
    int next_index;

    portENTER_CRITICAL(&player_bridge_mux);
    index = current_index;
    portEXIT_CRITICAL(&player_bridge_mux);

    next_index = clamp_track_index(index + 1);
    if (next_index < 0) {
        return false;
    }

    portENTER_CRITICAL(&player_bridge_mux);
    current_index = next_index;
    portEXIT_CRITICAL(&player_bridge_mux);
    return play_current(false);
}

bool play_prev_music(void)
{
    int index;
    int prev_index;

    portENTER_CRITICAL(&player_bridge_mux);
    index = current_index;
    portEXIT_CRITICAL(&player_bridge_mux);

    prev_index = clamp_track_index(index - 1);
    if (prev_index < 0) {
        return false;
    }

    portENTER_CRITICAL(&player_bridge_mux);
    current_index = prev_index;
    portEXIT_CRITICAL(&player_bridge_mux);
    return play_current(false);
}

bool pause_music(void)
{
    audio_player_callback_event_t state = speaker_player_get_state();

    if (speaker_player_is_paused()) {
        return speaker_player_request_resume() == ESP_OK;
    }

    if (state == AUDIO_PLAYER_CALLBACK_EVENT_PLAYING) {
        return speaker_player_request_pause() == ESP_OK;
    }

    return play_current(false);
}

bool set_volume_sta(int volume)
{
    int clamped = clamp_ui_volume(volume);
    int previous_volume;
    bool was_muted;

    portENTER_CRITICAL(&player_bridge_mux);
    previous_volume = ui_volume;
    was_muted = ui_muted;
    ui_volume = clamped;
    ui_muted = (clamped == 0);
    portEXIT_CRITICAL(&player_bridge_mux);

    if (clamped == 0) {
        return was_muted ? true : (speaker_codec_mute_set(true) == ESP_OK);
    }

    if (was_muted && speaker_codec_mute_set(false) != ESP_OK) {
        return false;
    }

    if (!was_muted && previous_volume == clamped) {
        return true;
    }

    return speaker_player_request_volume(map_ui_volume_to_codec(clamped)) == ESP_OK;
}

int get_volume(void)
{
    int volume;

    portENTER_CRITICAL(&player_bridge_mux);
    volume = ui_muted ? 0 : ui_volume;
    portEXIT_CRITICAL(&player_bridge_mux);

    return volume;
}
