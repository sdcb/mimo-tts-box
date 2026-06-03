#define WIN32_LEAN_AND_MEAN
#include "ui_request_flow.h"

#include <commctrl.h>
#include <stdlib.h>

#include "audio.h"
#include "config.h"
#include "history_store.h"
#include "http_client.h"
#include "request_builder.h"
#include "ui_controls.h"
#include "ui_dialogs.h"
#include "ui_history_list.h"
#include "ui_response_view.h"
#include "win32_helpers.h"

static void CALLBACK request_threadpool_proc(PTP_CALLBACK_INSTANCE instance, PVOID context) {
    (void)instance;
    RequestTask *task = (RequestTask *)context;
    RequestResult *result = (RequestResult *)calloc(1, sizeof(RequestResult));
    if (!result) {
        free_request_task(task);
        return;
    }
    result->audio_duration_ms = -1;
    result->prompt_tokens = -1;
    result->completion_tokens = -1;
    result->total_tokens = -1;
    result->request_id = task->request_id;
    result->list_index = task->list_index;
    result->history_dir = wcs_dup_or_empty(task->history_dir);
    result->request_time = wcs_dup_or_empty(task->request_time);
    result->url = wcs_dup_or_empty(task->url);
    result->audio_text = wcs_dup_or_empty(task->audio_text);
    result->output_format = wcs_dup_or_empty(task->output_format);
    result->auto_play_on_download = task->auto_play_on_download;
    result->request_json = str_dup_or_empty(task->request_json);

    HttpResponse http;
    BOOL transport_ok = http_post_json(task->url, task->api_key, task->request_json, &http);
    wchar_t response_time[64];
    current_display_time(response_time);
    result->response_time = wcs_dup_or_empty(response_time);
    result->status_code = http.status_code;
    result->elapsed_ms = http.elapsed_ms;
    if (http.body) {
        result->response_text = str_dup_or_empty(http.body);
    } else if (http.error_text) {
        char *err = utf16_to_utf8(http.error_text);
        result->response_text = err ? err : str_dup_or_empty("");
    } else {
        result->response_text = str_dup_or_empty("");
    }
    wchar_t *parse_error = NULL;
    if (transport_ok && http.status_code >= 200 && http.status_code < 300) {
        ParsedAudioResponse parsed;
        result->json_ok = audio_parse_response(result->response_text, task->output_format, &parsed, &parse_error);
        if (result->json_ok) {
            result->pcm_data = parsed.pcm_data;
            result->pcm_size = parsed.pcm_size;
            result->audio_duration_ms = parsed.audio_duration_ms;
            result->prompt_tokens = parsed.prompt_tokens;
            result->completion_tokens = parsed.completion_tokens;
            result->total_tokens = parsed.total_tokens;
            result->final_text_preview = parsed.final_text_preview;
            parsed.pcm_data = NULL;
            parsed.final_text_preview = NULL;
        }
        audio_free_parsed_response(&parsed);
        result->success = result->json_ok;
    }
    if (!result->success) {
        if (http.error_text) {
            result->error_text = wcs_dup_or_empty(http.error_text);
        } else if (parse_error) {
            result->error_text = wcs_dup_or_empty(parse_error);
        } else {
            result->error_text = utf8_to_utf16(result->response_text);
        }
    }
    free(parse_error);
    history_save_result(result);
    http_response_free(&http);
    /* Ownership of result moves to the UI thread and is released in ui_on_request_done. */
    PostMessageW(task->hwnd, WM_APP_REQUEST_DONE, 0, (LPARAM)result);
    free_request_task(task);
}

void ui_on_send(MainState *s) {
    ui_update_config_from_controls(s);
    if (!s->config->url || s->config->url[0] == L'\0') {
        MessageBoxW(s->hwnd, L"URL 不能为空。", APP_TITLE, MB_ICONWARNING);
        return;
    }
    wchar_t *save_error = NULL;
    config_save(s->config, &save_error);
    if (save_error) {
        MessageBoxW(s->hwnd, save_error, APP_TITLE, MB_ICONWARNING);
        free(save_error);
        save_error = NULL;
    }
    if (!config_has_api_key(s->config)) {
        if (!ui_show_config_dialog(s->hwnd, s->config) || !config_has_api_key(s->config)) {
            MessageBoxW(s->hwnd, L"发送请求前必须配置 API Key。", APP_TITLE, MB_ICONWARNING);
            return;
        }
        set_control_text(s->url_edit, s->config->url);
    }
    config_save(s->config, &save_error);
    if (save_error) {
        MessageBoxW(s->hwnd, save_error, APP_TITLE, MB_ICONWARNING);
        free(save_error);
    }
    char *json = build_request_json(&s->config->request);
    if (!json) {
        MessageBoxW(s->hwnd, L"构建请求 JSON 失败。", APP_TITLE, MB_ICONERROR);
        return;
    }
    wchar_t dir[32];
    wchar_t display_time[64];
    wchar_t list_time[64];
    current_timestamp(dir);
    current_display_time(display_time);
    ui_format_request_time_for_list(dir, display_time, list_time);
    LONG request_id = InterlockedIncrement(&s->next_request_id);
    s->latest_request_id = request_id;
    int index = ui_list_insert_record(s->list, list_time, L"--", L"--", s->config->request.audio_text, dir);
    ui_list_select_record(s->list, index);

    RequestTask *task = (RequestTask *)calloc(1, sizeof(RequestTask));
    if (!task) {
        free(json);
        return;
    }
    task->hwnd = s->hwnd;
    task->request_id = request_id;
    task->list_index = index;
    task->history_dir = wcs_dup_or_empty(dir);
    task->url = wcs_dup_or_empty(s->config->url);
    task->api_key = wcs_dup_or_empty(s->config->api_key);
    task->request_time = wcs_dup_or_empty(display_time);
    task->style_control = wcs_dup_or_empty(s->config->request.style_control);
    task->audio_text = wcs_dup_or_empty(s->config->request.audio_text);
    task->voice = wcs_dup_or_empty(s->config->request.voice);
    task->output_format = wcs_dup_or_empty(s->config->request.output_format);
    task->optimize_text_preview = s->config->request.optimize_text_preview;
    task->auto_play_on_download = s->config->request.auto_play_on_download;
    task->request_json = json;
    history_save_pending(task);
    if (!TrySubmitThreadpoolCallback(request_threadpool_proc, task, NULL)) {
        MessageBoxW(s->hwnd, L"无法启动请求线程。", APP_TITLE, MB_ICONERROR);
        free_request_task(task);
    }
}

void ui_on_request_done(MainState *s, RequestResult *result) {
    wchar_t elapsed[32];
    wchar_t status[32];
    swprintf(elapsed, ARRAYSIZE(elapsed), L"%lums", (unsigned long)result->elapsed_ms);
    if (result->status_code > 0) {
        swprintf(status, ARRAYSIZE(status), L"%lu", (unsigned long)result->status_code);
    } else {
        wcscpy(status, L"--");
    }
    int index = ui_list_find_by_dir(s->list, result->history_dir);
    if (index < 0) {
        index = result->list_index;
    }
    ListView_SetItemText(s->list, index, 1, elapsed);
    ListView_SetItemText(s->list, index, 2, status);
    ui_list_set_dir(s->list, index, result->history_dir);
    if (result->request_id == s->latest_request_id) {
        ui_set_active_from_result(s, result);
        ui_refresh_response_view(s, result->success);
        if (result->success && result->auto_play_on_download && s->active.audio.pcm_data && s->active.audio.pcm_size > 0) {
            ui_on_play(s);
        }
    }
    free_request_result(result);
}
