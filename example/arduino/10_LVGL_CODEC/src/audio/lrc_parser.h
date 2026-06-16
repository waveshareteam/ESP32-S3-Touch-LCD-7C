#pragma once

#include <stddef.h>
#include <stdint.h>

#include <FS.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LRC_MAX_LINES 128
#define LRC_MAX_TEXT_LENGTH 128

typedef struct {
    uint32_t time_ms;
    char text[LRC_MAX_TEXT_LENGTH];
} LrcLine;

typedef struct {
    size_t line_count;
    LrcLine lines[LRC_MAX_LINES];
} LrcData;

bool lrc_load(fs::FS &file_system, const char *file_path, LrcData *lrc_data);
int lrc_find_index(const LrcData *lrc_data, uint32_t time_ms);
const char *lrc_get_text(const LrcData *lrc_data, uint32_t time_ms);

#ifdef __cplusplus
}
#endif
