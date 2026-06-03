#pragma once

#include "ui_internal.h"

void ui_configure_list_columns(HWND list, int dpi);
void ui_format_request_time_for_list(const wchar_t *dir_name, const wchar_t *fallback, wchar_t out[64]);
int ui_list_insert_record(HWND list, const wchar_t *time, const wchar_t *elapsed, const wchar_t *status,
                          const wchar_t *text, const wchar_t *dir_name);
void ui_list_select_record(HWND list, int index);
wchar_t *ui_list_get_dir(HWND list, int index);
void ui_list_set_dir(HWND list, int index, const wchar_t *dir_name);
int ui_list_find_by_dir(HWND list, const wchar_t *dir_name);
/* List rows own duplicated history directory names in lParam until ui_clear_list_lparams. */
void ui_clear_list_lparams(HWND list);
void ui_refresh_request_time_column(HWND list);
void ui_load_history_into_list(MainState *s);
void ui_activate_history_record(MainState *s, int index);
