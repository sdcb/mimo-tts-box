#pragma once

#include "ui_internal.h"

HWND ui_make_child(MainState *s, const wchar_t *class_name, const wchar_t *text, DWORD style, DWORD exstyle, int id);
HWND ui_make_dialog_child(HWND parent, HINSTANCE instance, HFONT font, int dpi, DWORD exstyle,
                          const wchar_t *class_name, const wchar_t *text, DWORD style,
                          int x, int y, int width, int height, int id);
void ui_trigger_send_command(MainState *s);
BOOL ui_is_alt_s_message(UINT msg, WPARAM wparam, LPARAM lparam);
void ui_combo_select_text(HWND combo, const wchar_t *value);
wchar_t *ui_combo_get_text(HWND combo);
LRESULT ui_paint_label_on_window_background(WPARAM wparam);
void ui_populate_request_controls(MainState *s);
void ui_update_config_from_controls(MainState *s);
void ui_parse_request_into_controls(MainState *s, const char *request_json);
void ui_update_multiline_scrollbar(HWND edit);
void ui_refresh_multiline_scrollbars(MainState *s);
