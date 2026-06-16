#pragma once
#include "driver/i2c_master.h"
#include <stdbool.h>
#include <stdint.h>
#include <FS.h>


#ifdef __cplusplus
extern "C" {
#endif

bool audio_player_init(i2c_master_bus_handle_t bus_handle,
                       fs::FS &file_system,
                       const char *file_path,
                       uint8_t volume);

bool audio_player_play(const char *file_path);
void audio_player_loop(void);
bool audio_player_toggle_pause(void);
bool audio_player_is_paused(void);
bool audio_player_set_volume(uint8_t volume);
uint8_t audio_player_get_volume(void);


#ifdef __cplusplus
}
#endif
