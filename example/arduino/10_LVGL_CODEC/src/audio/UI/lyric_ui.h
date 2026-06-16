#pragma once

#include <stdint.h>
#include <FS.h>

void lyric_ui_init(void);
bool lyric_ui_load(fs::FS &file_system, const char *audio_path);
void lyric_ui_update(uint32_t elapsed_ms);
void lyric_ui_set_text(const char *text);

