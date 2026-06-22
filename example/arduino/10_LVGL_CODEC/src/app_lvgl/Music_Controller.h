#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool scan_music_dir(const char *base_path);
const char *get_file_path(void);
const char *get_file_name(const char *path);
int get_track_count(void);
int get_current_index(void);

bool play_current(bool start_paused);
bool play_next_music(void);
bool play_prev_music(void);
bool pause_music(void);

bool set_volume_sta(int volume);
int get_volume(void);

#ifdef __cplusplus
}
#endif
