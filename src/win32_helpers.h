#pragma once

#include "app_types.h"

wchar_t *wcs_dup_or_empty(const wchar_t *text);
char *str_dup_or_empty(const char *text);
void free_config(AppConfig *config);
void free_active_response(ActiveResponse *response);
void free_history_record(HistoryRecord *record);
void free_request_task(RequestTask *task);
void free_request_result(RequestResult *result);

char *utf16_to_utf8(const wchar_t *text);
wchar_t *utf8_to_utf16(const char *text);
wchar_t *format_win32_error(DWORD error_code);
wchar_t *format_hresult_error(HRESULT hr);
wchar_t *path_join2(const wchar_t *a, const wchar_t *b);
wchar_t *path_join3(const wchar_t *a, const wchar_t *b, const wchar_t *c);
BOOL ensure_directory(const wchar_t *path);
BOOL read_file_utf8(const wchar_t *path, char **out_text, DWORD *out_size);
BOOL write_file_utf8(const wchar_t *path, const char *text);
BOOL write_file_bytes(const wchar_t *path, const BYTE *data, DWORD size);
BOOL file_exists(const wchar_t *path);
void current_timestamp(wchar_t out[32]);
void current_display_time(wchar_t out[64]);
wchar_t *format_elapsed(DWORD elapsed_ms);
void set_control_text(HWND hwnd, const wchar_t *text);
wchar_t *get_control_text_alloc(HWND hwnd);
void safe_free(void *ptr);
