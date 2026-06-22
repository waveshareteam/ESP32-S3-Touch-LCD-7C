#include "Muisc_Player_UI.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "lvgl.h"
#include "Music_Controller.h"
#include "../lvgl_port/lvgl_port.h"

typedef struct {
    audio_player_callback_event_t state;
    bool paused;
    int volume;
    char file_path[256];
} app_audio_snapshot_t;

static portMUX_TYPE app_state_mux = portMUX_INITIALIZER_UNLOCKED;
static app_audio_snapshot_t audio_snapshot = {
    .state = AUDIO_PLAYER_CALLBACK_EVENT_IDLE,
    .paused = false,
    .volume = CODEC_DEFAULT_VOLUME,
    .file_path = {0},
};

static lv_obj_t *title_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *file_label = NULL;
static lv_obj_t *track_label = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *volume_value_label = NULL;
static bool volume_slider_dragging = false;
static bool volume_commit_pending = false;
static int pending_volume_value = CODEC_DEFAULT_VOLUME;
static uint32_t volume_interaction_tick = 0;
static char status_text_cache[64];
static char file_text_cache[160];
static char track_text_cache[32];
static char volume_text_cache[8];

#define VOLUME_INTERACTION_GUARD_MS  (180U)

static lv_obj_t *create_action_button(lv_obj_t *parent, const char *text)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_t *label = lv_label_create(button);

    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(button, 110, 48);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x2F80ED), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
    lv_obj_set_style_radius(button, 12, 0);
    lv_obj_set_style_pad_all(button, 0, 0);

    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    return button;
}

static const char *audio_state_to_string(audio_player_callback_event_t state, bool paused)
{
    if (paused) {
        return "Paused";
    }

    switch (state) {
        case AUDIO_PLAYER_CALLBACK_EVENT_PLAYING:
            return "Playing";
        case AUDIO_PLAYER_CALLBACK_EVENT_PAUSE:
            return "Paused";
        case AUDIO_PLAYER_CALLBACK_EVENT_COMPLETED_PLAYING_NEXT:
            return "Completed";
        case AUDIO_PLAYER_CALLBACK_EVENT_IDLE:
        default:
            return "Idle";
    }
}

static bool snapshot_changed(const app_audio_snapshot_t *lhs, const app_audio_snapshot_t *rhs)
{
    if (lhs->state != rhs->state || lhs->paused != rhs->paused || lhs->volume != rhs->volume) {
        return true;
    }

    return strcmp(lhs->file_path, rhs->file_path) != 0;
}

static bool snapshot_volume_only_changed(const app_audio_snapshot_t *lhs, const app_audio_snapshot_t *rhs)
{
    return lhs->volume != rhs->volume &&
           lhs->state == rhs->state &&
           lhs->paused == rhs->paused &&
           strcmp(lhs->file_path, rhs->file_path) == 0;
}

static bool volume_interaction_active(void)
{
    return lv_tick_elaps(volume_interaction_tick) < VOLUME_INTERACTION_GUARD_MS;
}

static void set_label_text_if_changed(lv_obj_t *label, char *cache, size_t cache_size, const char *text)
{
    if (label == NULL || text == NULL) {
        return;
    }

    if (strncmp(cache, text, cache_size) == 0) {
        return;
    }

    strncpy(cache, text, cache_size - 1);
    cache[cache_size - 1] = '\0';
    lv_label_set_text(label, cache);
}

static void refresh_volume_ui(int volume, bool sync_slider)
{
    char volume_text[8];

    if (volume_value_label != NULL) {
        snprintf(volume_text, sizeof(volume_text), "%d", volume);
        set_label_text_if_changed(volume_value_label, volume_text_cache, sizeof(volume_text_cache), volume_text);
    }

    if (sync_slider && volume_slider != NULL && lv_slider_get_value(volume_slider) != volume) {
        lv_slider_set_value(volume_slider, volume, LV_ANIM_OFF);
    }
}

static void queue_pending_volume_commit(int volume)
{
    portENTER_CRITICAL(&app_state_mux);
    pending_volume_value = volume;
    volume_commit_pending = true;
    portEXIT_CRITICAL(&app_state_mux);
}

static void refresh_player_ui(lv_timer_t *timer)
{
    app_audio_snapshot_t snapshot;
    char line_buf[160];
    int current_track;
    int track_count;

    (void) timer;

    portENTER_CRITICAL(&app_state_mux);
    snapshot = audio_snapshot;
    portEXIT_CRITICAL(&app_state_mux);

    snprintf(line_buf, sizeof(line_buf), "State: %s",audio_state_to_string(snapshot.state, snapshot.paused));
    set_label_text_if_changed(status_label, status_text_cache, sizeof(status_text_cache), line_buf);

    snprintf(line_buf, sizeof(line_buf), "File: %s", get_file_name(snapshot.file_path));
    set_label_text_if_changed(file_label, file_text_cache, sizeof(file_text_cache), line_buf);

    current_track = get_current_index();
    track_count = get_track_count();
    if (current_track >= 0 && track_count > 0) {
        snprintf(line_buf, sizeof(line_buf), "Track %d / %d", current_track + 1, track_count);
    } else {
        snprintf(line_buf, sizeof(line_buf), "Track 0 / 0");
    }
    set_label_text_if_changed(track_label, track_text_cache, sizeof(track_text_cache), line_buf);

    if (!volume_slider_dragging) {
        refresh_volume_ui(snapshot.volume, true);
    }
}

static void prev_btn_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        play_prev_music();
    }
}

static void play_btn_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        pause_music();
    }
}

static void next_btn_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) == LV_EVENT_CLICKED) {
        play_next_music();
    }
}

static void volume_slider_event_cb(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    int volume = lv_slider_get_value(lv_event_get_target(event));

    if (code == LV_EVENT_PRESSED) {
        volume_slider_dragging = true;
        volume_interaction_tick = lv_tick_get();
    }

    if (code == LV_EVENT_VALUE_CHANGED) {
        volume_slider_dragging = true;
        volume_interaction_tick = lv_tick_get();
        refresh_volume_ui(volume, false);
        queue_pending_volume_commit(volume);
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        volume_slider_dragging = false;
        volume_interaction_tick = lv_tick_get();
        refresh_volume_ui(volume, false);
        queue_pending_volume_commit(volume);
    }
}

static void layout_center_buttons(lv_obj_t *btn_1, lv_obj_t *btn_2, lv_obj_t *btn_3)
{
    if (btn_1 == NULL || btn_2 == NULL || btn_3 == NULL) {
        return;
    }

    lv_obj_align(btn_1, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(btn_2, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align(btn_3, LV_ALIGN_TOP_RIGHT, 0, 0);
}

void music_player_ui_init(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_t *surface = lv_obj_create(screen);
    lv_obj_t *controls = lv_obj_create(surface);
    lv_obj_t *prev_btn;
    lv_obj_t *play_btn;
    lv_obj_t *next_btn;
    lv_obj_t *volume_label;
    lv_obj_t *volume_panel;

    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    lv_obj_remove_style_all(surface);
    lv_obj_set_size(surface, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(surface, LV_OPA_TRANSP, 0);

    title_label = lv_label_create(surface);
    lv_label_set_text(title_label, "Music Player");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x111827), 0);
    lv_obj_set_style_text_font(title_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_LEFT, 24, 20);

    status_label = lv_label_create(surface);
    lv_label_set_text(status_label, "State: Idle");
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_text_font(status_label, LV_FONT_DEFAULT, 0);
    lv_obj_align_to(status_label, title_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);

    file_label = lv_label_create(surface);
    lv_label_set_long_mode(file_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(file_label, 720);
    lv_label_set_text(file_label, "File: (none)");
    lv_obj_set_style_text_color(file_label, lv_color_hex(0x2563EB), 0);
    lv_obj_set_style_text_font(file_label, LV_FONT_DEFAULT, 0);
    lv_obj_align_to(file_label, status_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 16);

    track_label = lv_label_create(surface);
    lv_label_set_text(track_label, "Track 0 / 0");
    lv_obj_set_style_text_color(track_label, lv_color_hex(0xB45309), 0);
    lv_obj_set_style_text_font(track_label, LV_FONT_DEFAULT, 0);
    lv_obj_align_to(track_label, file_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 12);

    lv_obj_set_size(controls, 420, 190);
    lv_obj_align(controls, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_style_bg_color(controls, lv_color_hex(0xF3F8FF), 0);
    lv_obj_set_style_bg_opa(controls, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(controls, 0, 0);
    lv_obj_set_style_radius(controls, 18, 0);
    lv_obj_set_style_pad_all(controls, 18, 0);
    lv_obj_clear_flag(controls, LV_OBJ_FLAG_SCROLLABLE);

    prev_btn = create_action_button(controls, "Prev");
    lv_obj_add_event_cb(prev_btn, prev_btn_event_cb, LV_EVENT_CLICKED, NULL);

    play_btn = create_action_button(controls, "Play/Pause");
    lv_obj_add_event_cb(play_btn, play_btn_event_cb, LV_EVENT_CLICKED, NULL);

    next_btn = create_action_button(controls, "Next");
    lv_obj_add_event_cb(next_btn, next_btn_event_cb, LV_EVENT_CLICKED, NULL);

    layout_center_buttons(prev_btn, play_btn, next_btn);

    volume_label = lv_label_create(controls);
    lv_label_set_text(volume_label, "Volume");
    lv_obj_set_style_text_color(volume_label, lv_color_hex(0x4B5563), 0);
    lv_obj_set_style_text_font(volume_label, LV_FONT_DEFAULT, 0);
    lv_obj_align(volume_label, LV_ALIGN_BOTTOM_LEFT, 8, -48);

    volume_panel = lv_obj_create(controls);
    lv_obj_remove_style_all(volume_panel);
    lv_obj_set_size(volume_panel, 60, 28);
    lv_obj_align(volume_panel, LV_ALIGN_BOTTOM_RIGHT, -8, -40);
    lv_obj_set_style_bg_color(volume_panel, lv_color_hex(0xE5EEFf), 0);
    lv_obj_set_style_bg_opa(volume_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(volume_panel, 10, 0);
    lv_obj_set_style_pad_all(volume_panel, 0, 0);
    lv_obj_clear_flag(volume_panel, LV_OBJ_FLAG_SCROLLABLE);

    volume_value_label = lv_label_create(volume_panel);
    lv_label_set_text(volume_value_label, "100");
    lv_obj_set_style_text_color(volume_value_label, lv_color_hex(0x1D4ED8), 0);
    lv_obj_set_style_text_font(volume_value_label, LV_FONT_DEFAULT, 0);
    lv_obj_center(volume_value_label);

    volume_slider = lv_slider_create(controls);
    lv_obj_set_size(volume_slider, 320, 14);
    lv_slider_set_range(volume_slider, 0, 100);
    lv_slider_set_value(volume_slider, 100, LV_ANIM_OFF);
    lv_obj_align(volume_slider, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0xD7E7FF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(volume_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(volume_slider, lv_color_hex(0x2F80ED), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(volume_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(volume_slider, lv_color_white(), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(volume_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(volume_slider, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_radius(volume_slider, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(volume_slider, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_add_event_cb(volume_slider, volume_slider_event_cb, LV_EVENT_ALL, NULL);

    refresh_player_ui(NULL);
}

void music_player_ui_set_audio_snapshot(audio_player_callback_event_t state, bool paused, int volume, const char *file_path)
{
    app_audio_snapshot_t next_snapshot;
    app_audio_snapshot_t previous_snapshot;
    bool changed;

    next_snapshot.state = state;
    next_snapshot.paused = paused;
    next_snapshot.volume = volume;
    if (file_path) {
        strncpy(next_snapshot.file_path, file_path, sizeof(next_snapshot.file_path) - 1);
        next_snapshot.file_path[sizeof(next_snapshot.file_path) - 1] = '\0';
    } else {
        next_snapshot.file_path[0] = '\0';
    }

    portENTER_CRITICAL(&app_state_mux);
    previous_snapshot = audio_snapshot;
    changed = snapshot_changed(&audio_snapshot, &next_snapshot);
    if (changed) {
        audio_snapshot = next_snapshot;
    }
    portEXIT_CRITICAL(&app_state_mux);

    if (!changed || title_label == NULL) {
        return;
    }

    if (snapshot_volume_only_changed(&previous_snapshot, &next_snapshot) &&
        (volume_slider_dragging || volume_interaction_active())) {
        return;
    }

    if (lvgl_port_lock(-1)) {
        refresh_player_ui(NULL);
        lvgl_port_unlock();
    }
}

bool music_player_ui_take_pending_volume(int *volume)
{
    bool has_pending = false;

    if (volume == NULL) {
        return false;
    }

    portENTER_CRITICAL(&app_state_mux);
    if (volume_commit_pending) {
        *volume = pending_volume_value;
        volume_commit_pending = false;
        has_pending = true;
    }
    portEXIT_CRITICAL(&app_state_mux);

    return has_pending;
}
