#include "player_ui.h"

#include "../../lvgl_port/lvgl_port.h"
#include "../audio_player.h"

namespace {

static lv_obj_t *player_panel = NULL;
static lv_obj_t *track_title_label = NULL;
static lv_obj_t *prev_btn = NULL;
static lv_obj_t *play_pause_btn = NULL;
static lv_obj_t *next_btn = NULL;
static lv_obj_t *play_pause_label = NULL;
static lv_obj_t *volume_slider = NULL;
static lv_obj_t *volume_value_label = NULL;
static player_ui_track_handler_t prev_track_handler = NULL;
static player_ui_track_handler_t next_track_handler = NULL;

static void style_control_button(lv_obj_t *button, uint32_t bg_color)
{
    lv_obj_set_size(button, 86, 52);
    lv_obj_set_style_radius(button, 10, 0);
    lv_obj_set_style_bg_color(button, lv_color_hex(bg_color), 0);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(button, 0, 0);
}

static void update_play_pause_text(void)
{
    if (play_pause_label == NULL) {
        return;
    }

    lv_label_set_text(play_pause_label,
                      audio_player_is_paused() ? "Resume" : "Pause");
}

static void update_volume_text(int volume)
{
    if (volume_value_label == NULL) {
        return;
    }

    lv_label_set_text_fmt(volume_value_label, "Volume %d", volume);
}

static void prev_btn_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || prev_track_handler == NULL) {
        return;
    }

    if (!prev_track_handler()) {
        return;
    }

    update_play_pause_text();
}

static void play_pause_btn_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }

    if (!audio_player_toggle_pause()) {
        return;
    }

    update_play_pause_text();
}

static void next_btn_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_CLICKED || next_track_handler == NULL) {
        return;
    }

    if (!next_track_handler()) {
        return;
    }

    update_play_pause_text();
}

static void volume_slider_event_cb(lv_event_t *event)
{
    if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *target = lv_event_get_target(event);
    int volume = lv_slider_get_value(target);

    if (!audio_player_set_volume((uint8_t)volume)) {
        return;
    }

    update_volume_text(volume);
}

static void disable_object_interaction(lv_obj_t *obj)
{
    if (obj == NULL) {
        return;
    }

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_MOMENTUM);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SNAPPABLE);
}

} // namespace

void player_ui_init(void)
{
    if (!lvgl_port_lock(-1)) {
        return;
    }

    lv_obj_t *screen = lv_scr_act();

    player_panel = lv_obj_create(screen);
    lv_obj_set_size(player_panel, LVGL_PORT_H_RES / 2, LVGL_PORT_V_RES);
    lv_obj_align(player_panel, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(player_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(player_panel, 0, 0);
    lv_obj_set_style_pad_all(player_panel, 24, 0);
    disable_object_interaction(player_panel);

    lv_obj_t *title_label = lv_label_create(player_panel);
    lv_label_set_text(title_label, "Player Control");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, -28, 48);
    disable_object_interaction(title_label);

    track_title_label = lv_label_create(player_panel);
    lv_label_set_text(track_title_label, "BGM_1");
    lv_obj_set_width(track_title_label, 220);
    lv_label_set_long_mode(track_title_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(track_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(track_title_label, lv_color_hex(0xE8E8E8), 0);
    lv_obj_align(track_title_label, LV_ALIGN_TOP_MID, -28, 88);
    disable_object_interaction(track_title_label);

    prev_btn = lv_btn_create(player_panel);
    style_control_button(prev_btn, 0x343434);
    lv_obj_align(prev_btn, LV_ALIGN_TOP_MID, -88, 162);
    lv_obj_add_event_cb(prev_btn, prev_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *prev_label = lv_label_create(prev_btn);
    lv_label_set_text(prev_label, "Prev");
    lv_obj_set_style_text_color(prev_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(prev_label);
    disable_object_interaction(prev_label);

    play_pause_btn = lv_btn_create(player_panel);
    style_control_button(play_pause_btn, 0x2F7CF6);
    lv_obj_set_size(play_pause_btn, 104, 52);
    lv_obj_align(play_pause_btn, LV_ALIGN_TOP_MID, 0, 162);
    lv_obj_add_event_cb(play_pause_btn, play_pause_btn_event_cb, LV_EVENT_CLICKED, NULL);

    play_pause_label = lv_label_create(play_pause_btn);
    lv_obj_set_style_text_color(play_pause_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_pause_label);
    disable_object_interaction(play_pause_label);
    update_play_pause_text();

    next_btn = lv_btn_create(player_panel);
    style_control_button(next_btn, 0x343434);
    lv_obj_align(next_btn, LV_ALIGN_TOP_MID, 88, 162);
    lv_obj_add_event_cb(next_btn, next_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *next_label = lv_label_create(next_btn);
    lv_label_set_text(next_label, "Next");
    lv_obj_set_style_text_color(next_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_label);
    disable_object_interaction(next_label);

    lv_obj_t *volume_label = lv_label_create(player_panel);
    lv_label_set_text(volume_label, "Volume");
    lv_obj_set_style_text_color(volume_label, lv_color_hex(0xC8C8C8), 0);
    lv_obj_align(volume_label, LV_ALIGN_TOP_RIGHT, -24, 112);
    disable_object_interaction(volume_label);

    volume_slider = lv_slider_create(player_panel);
    lv_obj_set_size(volume_slider, 12, 210);
    lv_obj_align(volume_slider, LV_ALIGN_TOP_RIGHT, -42, 146);
    lv_slider_set_range(volume_slider, 0, 21);
    lv_slider_set_value(volume_slider, audio_player_get_volume(), LV_ANIM_OFF);
    lv_slider_set_mode(volume_slider, LV_SLIDER_MODE_NORMAL);
    lv_obj_add_event_cb(volume_slider, volume_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    volume_value_label = lv_label_create(player_panel);
    lv_obj_set_style_text_color(volume_value_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(volume_value_label, LV_ALIGN_TOP_RIGHT, -12, 366);
    disable_object_interaction(volume_value_label);
    update_volume_text(audio_player_get_volume());

    lvgl_port_unlock();
}

void player_ui_set_track_handlers(player_ui_track_handler_t prev_handler,
                                  player_ui_track_handler_t next_handler)
{
    prev_track_handler = prev_handler;
    next_track_handler = next_handler;
}

void player_ui_set_track_title(const char *title)
{
    if (track_title_label == NULL || title == NULL) {
        return;
    }

    if (!lvgl_port_lock(-1)) {
        return;
    }

    lv_label_set_text(track_title_label, title);
    lvgl_port_unlock();
}

void player_ui_sync_state(void)
{
    if (!lvgl_port_lock(-1)) {
        return;
    }

    update_play_pause_text();
    if (volume_slider != NULL) {
        lv_slider_set_value(volume_slider, audio_player_get_volume(), LV_ANIM_OFF);
    }
    update_volume_text(audio_player_get_volume());

    lvgl_port_unlock();
}
