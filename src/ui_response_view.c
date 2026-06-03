#define WIN32_LEAN_AND_MEAN
#include "ui_response_view.h"

#include <commctrl.h>
#include <commdlg.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "ui_controls.h"
#include "win32_helpers.h"

static void format_number_n0(ULONGLONG value, wchar_t *out, size_t out_count) {
    wchar_t raw[32];
    swprintf(raw, ARRAYSIZE(raw), L"%llu", value);
    size_t len = wcslen(raw);
    size_t commas = len > 0 ? (len - 1) / 3 : 0;
    size_t needed = len + commas + 1;
    if (out_count < needed) {
        if (out_count > 0) {
            out[0] = L'\0';
        }
        return;
    }

    size_t out_pos = needed - 1;
    int group = 0;
    out[out_pos--] = L'\0';
    for (size_t i = len; i > 0; --i) {
        if (group == 3) {
            out[out_pos--] = L',';
            group = 0;
        }
        out[out_pos--] = raw[i - 1];
        ++group;
    }
}

static void format_milliseconds_n(DWORD elapsed_ms, wchar_t *out, size_t out_count) {
    wchar_t grouped[64];
    format_number_n0((ULONGLONG)elapsed_ms, grouped, ARRAYSIZE(grouped));
    swprintf(out, out_count, L"%ls ms", grouped);
}

static void format_optional_int_n0(int value, wchar_t *out, size_t out_count) {
    if (value < 0) {
        swprintf(out, out_count, L"--");
        return;
    }
    format_number_n0((ULONGLONG)value, out, out_count);
}

static void format_optional_milliseconds_n(int value, wchar_t *out, size_t out_count) {
    if (value < 0) {
        swprintf(out, out_count, L"--");
        return;
    }
    wchar_t grouped[64];
    format_number_n0((ULONGLONG)value, grouped, ARRAYSIZE(grouped));
    swprintf(out, out_count, L"%ls ms", grouped);
}

void ui_set_active_from_result(MainState *s, RequestResult *result) {
    free_active_response(&s->active);
    s->active.status_code = result->status_code;
    s->active.elapsed_ms = result->elapsed_ms;
    s->active.audio_duration_ms = result->audio_duration_ms;
    s->active.prompt_tokens = result->prompt_tokens;
    s->active.completion_tokens = result->completion_tokens;
    s->active.total_tokens = result->total_tokens;
    s->active.request_time = wcs_dup_or_empty(result->history_dir);
    s->active.response_time = wcs_dup_or_empty(result->response_time);
    s->active.preview_text = result->success && result->final_text_preview
                                 ? wcs_dup_or_empty(result->final_text_preview)
                                 : (result->error_text ? wcs_dup_or_empty(result->error_text) : utf8_to_utf16(result->response_text));
    s->active.raw_response = utf8_to_utf16(result->response_text ? result->response_text : "");
    s->active.audio.pcm_data = result->pcm_data;
    s->active.audio.pcm_size = result->pcm_size;
    wcsncpy(s->active.audio.timestamp, result->history_dir ? result->history_dir : L"", ARRAYSIZE(s->active.audio.timestamp) - 1);
    result->pcm_data = NULL;
}

void ui_set_response_status_bar_text(MainState *s, const wchar_t *status_code, const wchar_t *elapsed,
                                     const wchar_t *duration, const wchar_t *prompt_tokens,
                                     const wchar_t *completion_tokens, const wchar_t *total_tokens) {
    wchar_t part_text[STATUS_BAR_PART_COUNT][128];
    swprintf(part_text[0], ARRAYSIZE(part_text[0]), L"状态码: %ls", status_code ? status_code : L"--");
    swprintf(part_text[1], ARRAYSIZE(part_text[1]), L"响应时间: %ls", elapsed ? elapsed : L"--");
    swprintf(part_text[2], ARRAYSIZE(part_text[2]), L"音频时长: %ls", duration ? duration : L"--");
    swprintf(part_text[3], ARRAYSIZE(part_text[3]), L"Tokens: 输入 %ls / 输出 %ls / 总计 %ls",
             prompt_tokens ? prompt_tokens : L"--",
             completion_tokens ? completion_tokens : L"--",
             total_tokens ? total_tokens : L"--");
    for (int part = 0; part < STATUS_BAR_PART_COUNT; ++part) {
        SendMessageW(s->status_bar, SB_SETTEXT, (WPARAM)part, (LPARAM)part_text[part]);
    }
}

void ui_refresh_response_view(MainState *s, BOOL success_like) {
    wchar_t status_code[32];
    wchar_t elapsed[64];
    wchar_t duration[64];
    wchar_t prompt_tokens[32];
    wchar_t completion_tokens[32];
    wchar_t total_tokens[32];
    swprintf(status_code, ARRAYSIZE(status_code), L"%lu", (unsigned long)s->active.status_code);
    format_milliseconds_n(s->active.elapsed_ms, elapsed, ARRAYSIZE(elapsed));
    format_optional_milliseconds_n(s->active.audio_duration_ms, duration, ARRAYSIZE(duration));
    format_optional_int_n0(s->active.prompt_tokens, prompt_tokens, ARRAYSIZE(prompt_tokens));
    format_optional_int_n0(s->active.completion_tokens, completion_tokens, ARRAYSIZE(completion_tokens));
    format_optional_int_n0(s->active.total_tokens, total_tokens, ARRAYSIZE(total_tokens));
    ui_set_response_status_bar_text(s, status_code, s->active.elapsed_ms > 0 ? elapsed : L"--", duration,
                                    prompt_tokens, completion_tokens, total_tokens);
    set_control_text(s->preview_edit, success_like ? s->active.preview_text : (s->active.raw_response ? s->active.raw_response : s->active.preview_text));
    SendMessageW(s->preview_edit, WM_SETFONT, (WPARAM)(success_like ? s->ui_font : s->mono_font), TRUE);
    ui_update_multiline_scrollbar(s->preview_edit);
    EnableWindow(s->play_button, s->active.audio.pcm_data && s->active.audio.pcm_size > 0);
    EnableWindow(s->save_button, s->active.audio.pcm_data && s->active.audio.pcm_size > 0);
    EnableWindow(s->stop_button, FALSE);
}

void ui_layout_response_status_bar(MainState *s, const DpiMetrics *metrics, int x, int y, int width, int height) {
    int parts[STATUS_BAR_PART_COUNT];
    int status_w = metrics->status_code_w;
    int elapsed_w = metrics->status_elapsed_w;
    int duration_w = metrics->status_duration_w;
    if (width < status_w + elapsed_w + duration_w + metrics->status_tokens_min_w) {
        status_w = width / STATUS_BAR_PART_COUNT;
        elapsed_w = width / STATUS_BAR_PART_COUNT;
        duration_w = width / STATUS_BAR_PART_COUNT;
    }
    parts[0] = status_w;
    parts[1] = parts[0] + elapsed_w;
    parts[2] = parts[1] + duration_w;
    parts[3] = -1;
    SendMessageW(s->status_bar, SB_SETMINHEIGHT, (WPARAM)height, 0);
    MoveWindow(s->status_bar, x, y, width, height, TRUE);
    SendMessageW(s->status_bar, SB_SETPARTS, (WPARAM)STATUS_BAR_PART_COUNT, (LPARAM)parts);
}

void ui_on_play(MainState *s) {
    wchar_t *error = NULL;
    if (!audio_play(&s->active.audio, &error)) {
        MessageBoxW(s->hwnd, error ? error : L"播放失败。", APP_TITLE, MB_ICONERROR);
        free(error);
        return;
    }
    EnableWindow(s->play_button, FALSE);
    EnableWindow(s->stop_button, TRUE);
}

void ui_on_save(MainState *s) {
    if (!s->active.audio.pcm_data || s->active.audio.pcm_size == 0) {
        MessageBoxW(s->hwnd, L"没有可保存的音频。", APP_TITLE, MB_ICONWARNING);
        return;
    }
    wchar_t filename[MAX_PATH];
    swprintf(filename, ARRAYSIZE(filename), L"%ls.wav",
             s->active.audio.timestamp[0] ? s->active.audio.timestamp : L"mimo_tts");
    OPENFILENAMEW ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = s->hwnd;
    ofn.lpstrFilter = L"Audio Files (*.wav;*.mp3;*.aac)\0*.wav;*.mp3;*.aac\0WAV Audio (*.wav)\0*.wav\0MP3 Audio (*.mp3)\0*.mp3\0AAC Audio (*.aac)\0*.aac\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = ARRAYSIZE(filename);
    ofn.lpstrDefExt = L"wav";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    if (!GetSaveFileNameW(&ofn)) {
        return;
    }
    wchar_t *error = NULL;
    if (audio_save_to_file(&s->active.audio, filename, &error)) {
        MessageBoxW(s->hwnd, L"保存成功。", APP_TITLE, MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBoxW(s->hwnd, error ? error : L"保存失败。", APP_TITLE, MB_ICONERROR);
    }
    free(error);
}
