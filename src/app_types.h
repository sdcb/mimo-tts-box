#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <wchar.h>

#define APP_TITLE L"Mimo TTS Box"
#define DEFAULT_URL L"https://api.xiaomimimo.com/v1/chat/completions"
#define APPDATA_DIR L"AppData"

#define PCM_CHANNELS 1
#define PCM_SAMPLE_RATE 24000
#define PCM_BITS_PER_SAMPLE 16
#define PCM_BLOCK_ALIGN 2
#define PCM_AVG_BYTES_PER_SEC 48000

#define WM_APP_REQUEST_DONE (WM_APP + 10)
#define WM_APP_PLAYBACK_DONE (WM_APP + 11)

#define IDC_URL_EDIT 1001
#define IDC_SEND_BUTTON 1002
#define IDC_HISTORY_LIST 1003
#define IDC_STYLE_EDIT 1004
#define IDC_TEXT_EDIT 1005
#define IDC_VOICE_COMBO 1006
#define IDC_FORMAT_COMBO 1007
#define IDC_OPTIMIZE_CHECK 1008
#define IDC_STATUS_STATIC 1009
#define IDC_PREVIEW_EDIT 1010
#define IDC_PLAY_BUTTON 1011
#define IDC_STOP_BUTTON 1012
#define IDC_SAVE_BUTTON 1013
#define IDC_AUTO_PLAY_CHECK 1014

typedef struct RequestSettings {
    wchar_t *style_control;
    wchar_t *audio_text;
    wchar_t *voice;
    wchar_t *output_format;
    BOOL optimize_text_preview;
    BOOL auto_play_on_download;
} RequestSettings;

typedef struct AppConfig {
    wchar_t *url;
    wchar_t *api_key;
    RequestSettings request;
} AppConfig;

typedef struct AudioBuffer {
    BYTE *pcm_data;
    DWORD pcm_size;
    wchar_t timestamp[32];
} AudioBuffer;

typedef struct ParsedAudioResponse {
    BYTE *pcm_data;
    DWORD pcm_size;
    int audio_duration_ms;
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
    wchar_t *final_text_preview;
} ParsedAudioResponse;

typedef struct ActiveResponse {
    DWORD status_code;
    DWORD elapsed_ms;
    int audio_duration_ms;
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
    wchar_t *request_time;
    wchar_t *response_time;
    wchar_t *preview_text;
    wchar_t *raw_response;
    AudioBuffer audio;
} ActiveResponse;

typedef struct HistoryRecord {
    wchar_t *dir_name;
    wchar_t *request_time;
    wchar_t *response_time;
    wchar_t *elapsed_text;
    wchar_t *status_text;
    wchar_t *request_text;
    wchar_t *url;
    wchar_t *output_format;
} HistoryRecord;

typedef struct RequestTask {
    HWND hwnd;
    LONG request_id;
    int list_index;
    wchar_t *history_dir;
    wchar_t *url;
    wchar_t *api_key;
    wchar_t *request_time;
    wchar_t *style_control;
    wchar_t *audio_text;
    wchar_t *voice;
    wchar_t *output_format;
    BOOL optimize_text_preview;
    BOOL auto_play_on_download;
    char *request_json;
} RequestTask;

typedef struct RequestResult {
    LONG request_id;
    int list_index;
    wchar_t *history_dir;
    wchar_t *request_time;
    wchar_t *response_time;
    wchar_t *url;
    wchar_t *audio_text;
    wchar_t *output_format;
    char *request_json;
    char *response_text;
    wchar_t *error_text;
    wchar_t *final_text_preview;
    BYTE *pcm_data;
    DWORD pcm_size;
    int audio_duration_ms;
    int prompt_tokens;
    int completion_tokens;
    int total_tokens;
    DWORD status_code;
    DWORD elapsed_ms;
    BOOL auto_play_on_download;
    BOOL json_ok;
    BOOL success;
} RequestResult;
