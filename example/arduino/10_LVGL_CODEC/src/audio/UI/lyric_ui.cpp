#include "lyric_ui.h"

#include <Arduino.h>
#include <string.h>
#include "../Font/SC_Font_1.h"
#include "../lrc_parser.h"
#include "../../lvgl_port/lvgl_port.h"

enum {
    line_visible_count = 4,
    line_slot_count = 5,
    current_line_slot = 1,
};

typedef struct {
    lv_coord_t panel_width;
    lv_coord_t box_width;
    lv_coord_t box_height;
    lv_coord_t box_radius;
    lv_coord_t line_gap;
    lv_coord_t box_horizontal_padding;
    lv_coord_t box_top_padding;
    lv_coord_t box_bottom_padding;
    uint32_t scroll_anim_duration_ms;
} LyricLayout;

typedef struct {
    uint32_t time_ms;
    char primary_text[LRC_MAX_TEXT_LENGTH];
    char secondary_text[LRC_MAX_TEXT_LENGTH];
} LyricGroup;

typedef struct {
    lv_obj_t *panel;
    lv_obj_t *viewport;
    lv_obj_t *track;
    lv_obj_t *box[line_slot_count];
    lv_obj_t *primary_label[line_slot_count];
    lv_obj_t *secondary_label[line_slot_count];
} LyricUiObjects;

typedef struct {
    LrcData lrc;
    LyricGroup group[LRC_MAX_LINES];
    size_t group_count;
    bool ready;
    int current_group_index;
    int pending_group_index;
    uint32_t scroll_anim_start_ms;
    bool scroll_animating;
} LyricUiState;

static const LyricLayout lyric_layout = {
    .panel_width = LVGL_PORT_H_RES / 2,
    .box_width = 340,
    .box_height = 96,
    .box_radius = 8,
    .line_gap = 10,
    .box_horizontal_padding = 12,
    .box_top_padding = 10,
    .box_bottom_padding = 10,
    .scroll_anim_duration_ms = 280,
};

static LyricUiObjects lyric_ui = {0};
static LyricUiState lyric_state = {0};
static const char empty_line_text[] = " ";

static const LyricGroup *get_group(int group_index);

static void print_lrc_line(uint32_t time_ms, const char *text)
{
    uint32_t total_seconds = time_ms / 1000;
    uint32_t minutes = total_seconds / 60;
    uint32_t seconds = total_seconds % 60;
    uint32_t milliseconds = time_ms % 1000;

    Serial.printf("[lyric] %02lu:%02lu.%03lu %s\r\n",
                  (unsigned long)minutes,
                  (unsigned long)seconds,
                  (unsigned long)milliseconds,
                  text);
}

static lv_coord_t get_line_y(int slot)
{
    return (lv_coord_t)(slot * (lyric_layout.box_height + lyric_layout.line_gap));
}

static lv_coord_t get_line_step(void)
{
    return (lv_coord_t)(lyric_layout.box_height + lyric_layout.line_gap);
}

static lv_coord_t get_line_viewport_height(void)
{
    return (lv_coord_t)((lyric_layout.box_height * line_visible_count) +
                        (lyric_layout.line_gap * (line_visible_count - 1)));
}

static lv_coord_t get_line_track_height(void)
{
    return (lv_coord_t)((lyric_layout.box_height * line_slot_count) +
                        (lyric_layout.line_gap * (line_slot_count - 1)));
}

static lv_coord_t get_label_width(void)
{
    return (lv_coord_t)(lyric_layout.box_width - (lyric_layout.box_horizontal_padding * 2));
}

static lv_coord_t get_line_viewport_top(void)
{
    lv_coord_t viewport_height = get_line_viewport_height();
    if (viewport_height >= LVGL_PORT_V_RES) {
        return 0;
    }

    return (lv_coord_t)((LVGL_PORT_V_RES - viewport_height) / 2);
}

static lv_coord_t get_primary_label_height(void)
{
    return (lv_coord_t)(lyric_layout.box_height - 42);
}

static lv_coord_t get_secondary_label_height(void)
{
    return 32;
}

static void disable_object_interaction(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SNAPPABLE);
}

static bool text_has_cjk(const char *text)
{
    if (text == NULL) {
        return false;
    }

    while (*text != '\0') {
        if (((unsigned char)text[0] & 0xF0U) == 0xE0U) {
            return true;
        }
        ++text;
    }

    return false;
}

static void append_text_line(char *buffer, size_t buffer_size, const char *text)
{
    if (buffer == NULL || buffer_size == 0 || text == NULL || text[0] == '\0') {
        return;
    }

    size_t used_length = strlen(buffer);
    if (used_length >= buffer_size - 1) {
        return;
    }

    int written_length = snprintf(buffer + used_length,
                                  buffer_size - used_length,
                                  used_length > 0 ? "\n%s" : "%s",
                                  text);
    if (written_length < 0) {
        buffer[used_length] = '\0';
    }
}

static void set_line_group_text(int index, const char *primary_text, const char *secondary_text)
{
    if (index < 0 || index >= line_slot_count ||
        lyric_ui.primary_label[index] == NULL ||
        lyric_ui.secondary_label[index] == NULL) {
        return;
    }

    lv_label_set_text_static(lyric_ui.primary_label[index],
                             (primary_text != NULL && primary_text[0] != '\0') ? primary_text : empty_line_text);
    lv_label_set_text_static(lyric_ui.secondary_label[index],
                             (secondary_text != NULL && secondary_text[0] != '\0') ? secondary_text : empty_line_text);
}

static void set_line_slot_visible(int slot, bool visible)
{
    if (slot < 0 || slot >= line_slot_count || lyric_ui.box[slot] == NULL) {
        return;
    }

    if (visible) {
        lv_obj_clear_flag(lyric_ui.box[slot], LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lyric_ui.box[slot], LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_line_group_by_index(int slot, int group_index)
{
    const LyricGroup *group = get_group(group_index);
    bool has_text = false;

    if (group != NULL) {
        has_text = (group->primary_text[0] != '\0') || (group->secondary_text[0] != '\0');
    }

    set_line_slot_visible(slot, has_text);

    set_line_group_text(slot,
                        group != NULL ? group->primary_text : "",
                        group != NULL ? group->secondary_text : "");
}

static void apply_line_style(int slot)
{
    if (slot < 0 || slot >= line_slot_count ||
        lyric_ui.box[slot] == NULL ||
        lyric_ui.primary_label[slot] == NULL ||
        lyric_ui.secondary_label[slot] == NULL) {
        return;
    }

    if (slot == current_line_slot) {
        lv_obj_set_style_bg_color(lyric_ui.box[slot], lv_color_hex(0x1E1E1E), 0);
        lv_obj_set_style_bg_opa(lyric_ui.box[slot], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(lyric_ui.box[slot], 2, 0);
        lv_obj_set_style_border_color(lyric_ui.box[slot], lv_color_hex(0x3FA9F5), 0);
        lv_obj_set_style_text_color(lyric_ui.primary_label[slot], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_color(lyric_ui.secondary_label[slot], lv_color_hex(0xD0D0D0), 0);
    } else {
        lv_obj_set_style_bg_color(lyric_ui.box[slot], lv_color_hex(0x121212), 0);
        lv_obj_set_style_bg_opa(lyric_ui.box[slot], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(lyric_ui.box[slot], 0, 0);
        lv_obj_set_style_text_color(lyric_ui.primary_label[slot], lv_color_hex(0x9A9A9A), 0);
        lv_obj_set_style_text_color(lyric_ui.secondary_label[slot], lv_color_hex(0x707070), 0);
    }

    lv_obj_set_style_text_font(lyric_ui.primary_label[slot], &SC_Font_1, 0);
    lv_obj_set_style_text_font(lyric_ui.secondary_label[slot], &lv_font_montserrat_14, 0);
}

static void reset_line_positions(void)
{
    if (lyric_ui.track != NULL) {
        lv_obj_set_y(lyric_ui.track, 0);
    }

    for (int i = 0; i < line_slot_count; ++i) {
        if (lyric_ui.box[i] != NULL) {
            lv_obj_set_y(lyric_ui.box[i], get_line_y(i));
        }
    }
}

static void rotate_line_slots(void)
{
    lv_obj_t *first_box = lyric_ui.box[0];
    lv_obj_t *first_primary_label = lyric_ui.primary_label[0];
    lv_obj_t *first_secondary_label = lyric_ui.secondary_label[0];

    for (int i = 0; i < line_slot_count - 1; ++i) {
        lyric_ui.box[i] = lyric_ui.box[i + 1];
        lyric_ui.primary_label[i] = lyric_ui.primary_label[i + 1];
        lyric_ui.secondary_label[i] = lyric_ui.secondary_label[i + 1];
    }

    lyric_ui.box[line_slot_count - 1] = first_box;
    lyric_ui.primary_label[line_slot_count - 1] = first_primary_label;
    lyric_ui.secondary_label[line_slot_count - 1] = first_secondary_label;
}

static void build_group_from_lrc(int line_start, LyricGroup *group)
{
    if (group == NULL) {
        return;
    }

    group->time_ms = 0;
    group->primary_text[0] = '\0';
    group->secondary_text[0] = '\0';

    if (line_start < 0 || line_start >= (int)lyric_state.lrc.line_count) {
        return;
    }

    uint32_t time_ms = lyric_state.lrc.lines[line_start].time_ms;
    group->time_ms = time_ms;

    for (int i = line_start; i < (int)lyric_state.lrc.line_count; ++i) {
        if (lyric_state.lrc.lines[i].time_ms != time_ms) {
            break;
        }

        if (lyric_state.lrc.lines[i].text[0] == '\0') {
            continue;
        }

        if (group->primary_text[0] == '\0') {
            snprintf(group->primary_text, sizeof(group->primary_text), "%s", lyric_state.lrc.lines[i].text);
            continue;
        }

        if (group->secondary_text[0] == '\0') {
            bool current_is_cjk = text_has_cjk(group->primary_text);
            bool incoming_is_cjk = text_has_cjk(lyric_state.lrc.lines[i].text);

            if (!current_is_cjk && incoming_is_cjk) {
                char temp_text[LRC_MAX_TEXT_LENGTH] = {0};
                snprintf(temp_text, sizeof(temp_text), "%s", group->primary_text);
                snprintf(group->primary_text, sizeof(group->primary_text), "%s", lyric_state.lrc.lines[i].text);
                snprintf(group->secondary_text, sizeof(group->secondary_text), "%s", temp_text);
            } else {
                snprintf(group->secondary_text, sizeof(group->secondary_text), "%s", lyric_state.lrc.lines[i].text);
            }
            continue;
        }

        append_text_line(group->secondary_text,
                         sizeof(group->secondary_text),
                         lyric_state.lrc.lines[i].text);
    }
}

static void rebuild_lyric_group(void)
{
    lyric_state.group_count = 0;

    for (int i = 0; i < (int)lyric_state.lrc.line_count && lyric_state.group_count < LRC_MAX_LINES; ) {
        uint32_t time_ms = lyric_state.lrc.lines[i].time_ms;
        build_group_from_lrc(i, &lyric_state.group[lyric_state.group_count]);
        ++lyric_state.group_count;

        while (i < (int)lyric_state.lrc.line_count &&
               lyric_state.lrc.lines[i].time_ms == time_ms) {
            ++i;
        }
    }
}

static int find_group_index(uint32_t elapsed_ms)
{
    int group_index = -1;

    for (size_t i = 0; i < lyric_state.group_count; ++i) {
        if (lyric_state.group[i].time_ms > elapsed_ms) {
            break;
        }
        group_index = (int)i;
    }

    return group_index;
}

static const LyricGroup *get_group(int group_index)
{
    if (group_index < 0 || group_index >= (int)lyric_state.group_count) {
        return NULL;
    }

    return &lyric_state.group[group_index];
}

static void sync_group_window(int focus_group_index)
{
    for (int i = 0; i < line_slot_count; ++i) {
        int group_index = focus_group_index + i - current_line_slot;
        set_line_group_by_index(i, group_index);
    }

    for (int i = 0; i < line_slot_count; ++i) {
        apply_line_style(i);
    }

    reset_line_positions();
}

static void start_scroll_animation(uint32_t elapsed_ms, int next_group_index_value)
{
    if (next_group_index_value < 0) {
        return;
    }

    lyric_state.pending_group_index = next_group_index_value;
    lyric_state.scroll_anim_start_ms = elapsed_ms;
    lyric_state.scroll_animating = true;
}

static void update_scroll_animation(uint32_t elapsed_ms)
{
    if (!lyric_state.scroll_animating) {
        return;
    }

    uint32_t anim_elapsed_ms = elapsed_ms - lyric_state.scroll_anim_start_ms;
    float progress = (float)anim_elapsed_ms / (float)lyric_layout.scroll_anim_duration_ms;
    if (progress > 1.0f) {
        progress = 1.0f;
    }

    float eased = 1.0f - ((1.0f - progress) * (1.0f - progress));
    if (lyric_ui.track != NULL) {
        lv_coord_t offset_y = (lv_coord_t)(get_line_step() * eased);
        lv_obj_set_y(lyric_ui.track, -offset_y);
    }

    if (progress < 1.0f) {
        return;
    }

    lyric_state.scroll_animating = false;
    rotate_line_slots();
    lyric_state.current_group_index = lyric_state.pending_group_index;
    lyric_state.pending_group_index = -1;
    sync_group_window(lyric_state.current_group_index);
    print_lrc_line(lyric_state.group[lyric_state.current_group_index].time_ms,
                   lyric_state.group[lyric_state.current_group_index].primary_text);
}

static bool make_lrc_path(const char *audio_path, char *lrc_path, size_t path_size)
{
    if (audio_path == NULL || lrc_path == NULL || path_size == 0) {
        return false;
    }

    size_t path_length = strlen(audio_path);
    if (path_length + 1 > path_size) {
        return false;
    }

    snprintf(lrc_path, path_size, "%s", audio_path);
    char *extension = strrchr(lrc_path, '.');
    if (extension == NULL) {
        return false;
    }

    snprintf(extension, path_size - (size_t)(extension - lrc_path), ".lrc");
    return true;
}

void lyric_ui_init(void)
{
    if (!lvgl_port_lock(-1)) {
        return;
    }

    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lyric_ui.panel = lv_obj_create(screen);
    lv_obj_set_size(lyric_ui.panel, lyric_layout.panel_width, LVGL_PORT_V_RES);
    lv_obj_align(lyric_ui.panel, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(lyric_ui.panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lyric_ui.panel, 0, 0);
    lv_obj_set_style_pad_all(lyric_ui.panel, 0, 0);
    lv_obj_set_scrollbar_mode(lyric_ui.panel, LV_SCROLLBAR_MODE_OFF);
    disable_object_interaction(lyric_ui.panel);

    lyric_ui.viewport = lv_obj_create(lyric_ui.panel);
    lv_obj_set_size(lyric_ui.viewport, lyric_layout.box_width, get_line_viewport_height());
    lv_obj_set_x(lyric_ui.viewport, (lyric_layout.panel_width - lyric_layout.box_width) / 2);
    lv_obj_set_y(lyric_ui.viewport, get_line_viewport_top());
    lv_obj_set_style_bg_opa(lyric_ui.viewport, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lyric_ui.viewport, 0, 0);
    lv_obj_set_style_pad_all(lyric_ui.viewport, 0, 0);
    lv_obj_set_scrollbar_mode(lyric_ui.viewport, LV_SCROLLBAR_MODE_OFF);
    disable_object_interaction(lyric_ui.viewport);

    lyric_ui.track = lv_obj_create(lyric_ui.viewport);
    lv_obj_set_size(lyric_ui.track, lyric_layout.box_width, get_line_track_height());
    lv_obj_set_x(lyric_ui.track, 0);
    lv_obj_set_y(lyric_ui.track, 0);
    lv_obj_set_style_bg_opa(lyric_ui.track, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lyric_ui.track, 0, 0);
    lv_obj_set_style_pad_all(lyric_ui.track, 0, 0);
    lv_obj_set_scrollbar_mode(lyric_ui.track, LV_SCROLLBAR_MODE_OFF);
    disable_object_interaction(lyric_ui.track);

    for (int i = 0; i < line_slot_count; ++i) {
        lyric_ui.box[i] = lv_obj_create(lyric_ui.track);
        lv_obj_set_size(lyric_ui.box[i], lyric_layout.box_width, lyric_layout.box_height);
        lv_obj_set_x(lyric_ui.box[i], 0);
        lv_obj_set_y(lyric_ui.box[i], get_line_y(i));
        lv_obj_set_style_radius(lyric_ui.box[i], lyric_layout.box_radius, 0);
        lv_obj_set_style_border_width(lyric_ui.box[i], 0, 0);
        lv_obj_set_style_pad_left(lyric_ui.box[i], lyric_layout.box_horizontal_padding, 0);
        lv_obj_set_style_pad_right(lyric_ui.box[i], lyric_layout.box_horizontal_padding, 0);
        lv_obj_set_style_pad_top(lyric_ui.box[i], lyric_layout.box_top_padding, 0);
        lv_obj_set_style_pad_bottom(lyric_ui.box[i], lyric_layout.box_bottom_padding, 0);
        lv_obj_set_scrollbar_mode(lyric_ui.box[i], LV_SCROLLBAR_MODE_OFF);
        disable_object_interaction(lyric_ui.box[i]);

        lyric_ui.primary_label[i] = lv_label_create(lyric_ui.box[i]);
        lv_obj_set_width(lyric_ui.primary_label[i], get_label_width());
        lv_obj_set_height(lyric_ui.primary_label[i], get_primary_label_height());
        lv_label_set_long_mode(lyric_ui.primary_label[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(lyric_ui.primary_label[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lyric_ui.primary_label[i], LV_ALIGN_TOP_MID, 0, 4);
        disable_object_interaction(lyric_ui.primary_label[i]);

        lyric_ui.secondary_label[i] = lv_label_create(lyric_ui.box[i]);
        lv_obj_set_width(lyric_ui.secondary_label[i], get_label_width());
        lv_obj_set_height(lyric_ui.secondary_label[i], get_secondary_label_height());
        lv_label_set_long_mode(lyric_ui.secondary_label[i], LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(lyric_ui.secondary_label[i], LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(lyric_ui.secondary_label[i], LV_ALIGN_BOTTOM_MID, 0, -2);
        disable_object_interaction(lyric_ui.secondary_label[i]);
    }

    sync_group_window(-1);
    set_line_group_text(current_line_slot, "Song not loaded", "");

    lvgl_port_unlock();
}

void lyric_ui_set_text(const char *text)
{
    if (lyric_ui.panel == NULL || text == NULL) {
        return;
    }

    if (!lvgl_port_lock(-1)) {
        return;
    }

    lyric_state.scroll_animating = false;
    lyric_state.pending_group_index = -1;
    lyric_state.current_group_index = -1;
    for (int i = 0; i < line_slot_count; ++i) {
        set_line_slot_visible(i, false);
    }
    set_line_group_text(0, "", "");
    set_line_group_text(current_line_slot, text, "");
    set_line_group_text(2, "", "");
    set_line_group_text(3, "", "");
    set_line_group_text(4, "", "");
    set_line_slot_visible(current_line_slot, true);
    for (int i = 0; i < line_slot_count; ++i) {
        apply_line_style(i);
    }
    reset_line_positions();

    lvgl_port_unlock();
}

bool lyric_ui_load(fs::FS &file_system, const char *audio_path)
{
    char lrc_path[128] = {0};

    lyric_state.ready = false;
    lyric_state.current_group_index = -1;
    lyric_state.pending_group_index = -1;
    lyric_state.scroll_animating = false;

    if (!make_lrc_path(audio_path, lrc_path, sizeof(lrc_path))) {
        lyric_ui_set_text("Invalid lyrics path");
        return false;
    }

    if (!lrc_load(file_system, lrc_path, &lyric_state.lrc)) {
        lyric_ui_set_text("Lyrics not found");
        return false;
    }

    rebuild_lyric_group();
    lyric_state.ready = true;
    lyric_ui_set_text("Song loaded successfully");
    Serial.printf("[lyric] loaded: %s, lines=%u, groups=%u\r\n",
                  lrc_path,
                  (unsigned int)lyric_state.lrc.line_count,
                  (unsigned int)lyric_state.group_count);
    return true;
}

void lyric_ui_update(uint32_t elapsed_ms)
{
    if (!lyric_state.ready) {
        return;
    }

    if (lvgl_port_lock(-1)) {
        update_scroll_animation(elapsed_ms);
        lvgl_port_unlock();
    }

    if (lyric_state.scroll_animating) {
        return;
    }

    int next_group_index_value = find_group_index(elapsed_ms);
    if (next_group_index_value < 0 || next_group_index_value == lyric_state.current_group_index) {
        return;
    }

    if (lyric_state.current_group_index < 0) {
        lyric_state.current_group_index = next_group_index_value;
        if (lvgl_port_lock(-1)) {
            sync_group_window(lyric_state.current_group_index);
            lvgl_port_unlock();
        }

        print_lrc_line(lyric_state.group[lyric_state.current_group_index].time_ms,
                       lyric_state.group[lyric_state.current_group_index].primary_text);
        return;
    }

    if (next_group_index_value == lyric_state.current_group_index + 1) {
        if (lvgl_port_lock(-1)) {
            start_scroll_animation(elapsed_ms, next_group_index_value);
            lvgl_port_unlock();
        }
        return;
    } else {
        lyric_state.current_group_index = next_group_index_value;
        if (lvgl_port_lock(-1)) {
            sync_group_window(lyric_state.current_group_index);
            lvgl_port_unlock();
        }
    }

    print_lrc_line(lyric_state.group[lyric_state.current_group_index].time_ms,
                   lyric_state.group[lyric_state.current_group_index].primary_text);
}
