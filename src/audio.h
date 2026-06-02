#pragma once

#include "app_types.h"

BOOL audio_system_init(HWND hwnd, wchar_t **error_text);
void audio_system_shutdown(void);
BOOL audio_parse_response(const char *response_text, BYTE **audio_data, DWORD *audio_size,
                          wchar_t **final_preview, wchar_t **error_text);
BOOL audio_play(const AudioBuffer *buffer, wchar_t **error_text);
void audio_stop(void);
BOOL audio_is_playing(void);
BOOL audio_save_to_file(const AudioBuffer *buffer, const wchar_t *path, wchar_t **error_text);
