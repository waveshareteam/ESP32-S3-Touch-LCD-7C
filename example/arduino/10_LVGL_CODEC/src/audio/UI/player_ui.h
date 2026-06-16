#pragma once

typedef bool (*player_ui_track_handler_t)(void);

void player_ui_init(void);
void player_ui_set_track_handlers(player_ui_track_handler_t prev_handler,
                                  player_ui_track_handler_t next_handler);
void player_ui_set_track_title(const char *title);
void player_ui_sync_state(void);
