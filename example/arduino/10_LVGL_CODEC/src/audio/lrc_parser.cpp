#include "lrc_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool is_metadata_line(const String &line)
{
    return line.startsWith("[ti:") ||
           line.startsWith("[ar:") ||
           line.startsWith("[al:") ||
           line.startsWith("[by:") ||
           line.startsWith("[offset:");
}

static bool parse_decimal_number(const char *text, int *value, int *consumed_chars)
{
    int result = 0;
    int count = 0;

    while (isdigit((unsigned char)text[count])) {
        result = (result * 10) + (text[count] - '0');
        ++count;
    }

    if (count == 0) {
        return false;
    }

    *value = result;
    *consumed_chars = count;
    return true;
}

static uint32_t parse_fraction_ms(const char *text)
{
    int digits[3] = {0, 0, 0};
    int count = 0;

    while (isdigit((unsigned char)text[count]) && count < 3) {
        digits[count] = text[count] - '0';
        ++count;
    }

    if (count == 1) {
        return (uint32_t)(digits[0] * 100);
    }
    if (count == 2) {
        return (uint32_t)(digits[0] * 100 + digits[1] * 10);
    }
    if (count == 3) {
        return (uint32_t)(digits[0] * 100 + digits[1] * 10 + digits[2]);
    }
    return 0;
}

static bool parse_time_tag(const char *tag_text, uint32_t *time_ms)
{
    int minutes = 0;
    int seconds = 0;
    int consumed_chars = 0;
    const char *cursor = tag_text;

    if (!parse_decimal_number(cursor, &minutes, &consumed_chars)) {
        return false;
    }
    cursor += consumed_chars;

    if (*cursor != ':') {
        return false;
    }
    ++cursor;

    if (!parse_decimal_number(cursor, &seconds, &consumed_chars)) {
        return false;
    }
    cursor += consumed_chars;

    *time_ms = (uint32_t)((minutes * 60 + seconds) * 1000);

    if (*cursor == '.' || *cursor == ':') {
        ++cursor;
        *time_ms += parse_fraction_ms(cursor);
    }

    return true;
}

static void normalize_lrc_text(String *text)
{
    if (text == NULL) {
        return;
    }

    text->replace("：", ":");
    text->replace("﹕", ":");
    text->replace("∶", ":");
}

static bool lrc_parse_line(const String &line, LrcLine *lrc_line)
{
    if (!line.startsWith("[")) {
        return false;
    }

    int right_bracket = line.indexOf(']');
    if (right_bracket <= 1) {
        return false;
    }

    String tag_text = line.substring(1, right_bracket);
    if (tag_text.length() == 0 || !isdigit((unsigned char)tag_text[0])) {
        return false;
    }

    uint32_t time_ms = 0;
    if (!parse_time_tag(tag_text.c_str(), &time_ms)) {
        return false;
    }

    String lyric_text = line.substring(right_bracket + 1);
    lyric_text.trim();
    normalize_lrc_text(&lyric_text);

    lrc_line->time_ms = time_ms;
    lyric_text.toCharArray(lrc_line->text, sizeof(lrc_line->text));
    return true;
}

bool lrc_load(fs::FS &file_system, const char *file_path, LrcData *lrc_data)
{
    if (file_path == NULL || lrc_data == NULL) {
        return false;
    }

    File lrc_file = file_system.open(file_path, FILE_READ);
    if (!lrc_file) {
        return false;
    }

    lrc_data->line_count = 0;

    while (lrc_file.available() && lrc_data->line_count < LRC_MAX_LINES) {
        String line = lrc_file.readStringUntil('\n');
        line.trim();

        if (line.length() == 0 || is_metadata_line(line)) {
            continue;
        }

        LrcLine parsed_line = {};
        if (!lrc_parse_line(line, &parsed_line)) {
            continue;
        }

        lrc_data->lines[lrc_data->line_count++] = parsed_line;
    }

    lrc_file.close();
    return (lrc_data->line_count > 0);
}

int lrc_find_index(const LrcData *lrc_data, uint32_t time_ms)
{
    if (lrc_data == NULL || lrc_data->line_count == 0) {
        return -1;
    }

    int current_index = -1;
    for (size_t i = 0; i < lrc_data->line_count; ++i) {
        if (lrc_data->lines[i].time_ms > time_ms) {
            break;
        }
        current_index = (int)i;
    }

    return current_index;
}

const char *lrc_get_text(const LrcData *lrc_data, uint32_t time_ms)
{
    int line_index = lrc_find_index(lrc_data, time_ms);
    if (line_index < 0) {
        return "";
    }

    return lrc_data->lines[line_index].text;
}
