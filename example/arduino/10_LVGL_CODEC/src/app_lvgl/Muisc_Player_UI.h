#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../speaker_microphone/codec_dev.h"

#ifdef __cplusplus
extern "C" {
#endif

void music_player_ui_init(void);
void music_player_ui_set_audio_snapshot(audio_player_callback_event_t state, bool paused, int volume, const char *file_path);
bool music_player_ui_take_pending_volume(int *volume);

#ifdef __cplusplus
}
#endif
