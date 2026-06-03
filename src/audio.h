#pragma once

#include "app_types.h"

BOOL audio_system_init(HWND hwnd, wchar_t **error_text);
void audio_system_shutdown(void);
void audio_free_parsed_response(ParsedAudioResponse *parsed);
BOOL audio_parse_response(const char *response_text, const wchar_t *input_format,
                          ParsedAudioResponse *parsed, wchar_t **error_text);
BOOL audio_play(const AudioBuffer *buffer, wchar_t **error_text);
void audio_stop(void);
BOOL audio_is_playing(void);
BOOL audio_save_to_file(const AudioBuffer *buffer, const wchar_t *path, wchar_t **error_text);
