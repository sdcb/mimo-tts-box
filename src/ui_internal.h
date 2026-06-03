#pragma once

#include "app_types.h"

/* Internal UI state shared by the ui_*.c modules; keep it out of the public UI API. */
#define REQUEST_TIME_TIMER_ID 1
#define REQUEST_TIME_REFRESH_MS 60000
#define SPLITTER_HIT_SIZE 6
#define SPLITTER_LINE_SIZE 1
#define STATUS_BAR_PART_COUNT 4

typedef enum DragMode {
    DRAG_NONE,
    DRAG_MAIN,
    DRAG_VERTICAL,
    DRAG_REQUEST
} DragMode;

typedef struct MainState {
    HINSTANCE instance;
    HWND hwnd;
    AppConfig *config;
    int dpi;
    HFONT ui_font;
    HFONT emoji_font;
    HFONT mono_font;
    LONG next_request_id;
    LONG latest_request_id;
    int main_split;
    int vertical_split;
    int request_split;
    DragMode drag;
    HWND url_edit;
    HWND send_button;
    HWND list;
    HWND style_label;
    HWND text_label;
    HWND style_edit;
    HWND text_edit;
    HWND voice_combo;
    HWND format_combo;
    HWND optimize_check;
    HWND auto_play_check;
    HWND status_bar;
    HWND preview_edit;
    HWND play_button;
    HWND stop_button;
    HWND save_button;
    ActiveResponse active;
} MainState;

typedef struct ConfigDialogState {
    HWND hwnd;
    HFONT ui_font;
    int dpi;
    HWND url_edit;
    HWND api_edit;
    AppConfig *config;
    BOOL accepted;
    BOOL done;
} ConfigDialogState;

typedef struct DpiMetrics {
    int margin;
    int url_h;
    int min_w;
    int min_h;
    int window_w;
    int window_h;
    int send_w;
    int splitter;
    int splitter_line;
    int initial_main_split;
    int initial_vertical_split;
    int initial_request_split;
    int main_split_min;
    int main_right_min;
    int split_pane_min;
    int label_h;
    int bottom_controls_h;
    int controls_y_offset;
    int combo_drop_h;
    int voice_w;
    int format_x;
    int format_w;
    int optimize_x;
    int optimize_w;
    int auto_play_x;
    int auto_play_w;
    int check_h;
    int resp_button_w;
    int resp_button_h;
    int resp_button_step;
    int resp_gap;
    int status_h;
    int status_code_w;
    int status_elapsed_w;
    int status_duration_w;
    int status_tokens_min_w;
} DpiMetrics;
