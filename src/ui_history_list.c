#define WIN32_LEAN_AND_MEAN
#include "ui_history_list.h"

#include <commctrl.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "history_store.h"
#include "ui_controls.h"
#include "ui_metrics.h"
#include "ui_response_view.h"
#include "win32_helpers.h"

void ui_configure_list_columns(HWND list, int dpi) {
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t *names[] = { L"请求时间", L"耗时", L"状态码", L"请求文本" };
    int widths[] = { 96, 72, 70, 360 };
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNW col;
        memset(&col, 0, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = (LPWSTR)names[i];
        col.cx = ui_scale_for_dpi(widths[i], dpi);
        col.iSubItem = i;
        ListView_InsertColumn(list, i, &col);
    }
}

static BOOL parse_fixed_word(const wchar_t *text, int offset, int len, WORD *value) {
    WORD v = 0;
    if (!text) {
        return FALSE;
    }
    for (int i = 0; i < len; ++i) {
        wchar_t ch = text[offset + i];
        if (ch < L'0' || ch > L'9') {
            return FALSE;
        }
        v = (WORD)(v * 10 + (ch - L'0'));
    }
    *value = v;
    return TRUE;
}

static BOOL parse_history_dir_time(const wchar_t *dir_name, SYSTEMTIME *st) {
    if (!dir_name || wcslen(dir_name) < 15 || dir_name[8] != L'_') {
        return FALSE;
    }
    memset(st, 0, sizeof(*st));
    return parse_fixed_word(dir_name, 0, 4, &st->wYear) &&
           parse_fixed_word(dir_name, 4, 2, &st->wMonth) &&
           parse_fixed_word(dir_name, 6, 2, &st->wDay) &&
           parse_fixed_word(dir_name, 9, 2, &st->wHour) &&
           parse_fixed_word(dir_name, 11, 2, &st->wMinute) &&
           parse_fixed_word(dir_name, 13, 2, &st->wSecond);
}

static int days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned mp = (unsigned)(m + (m > 2 ? -3 : 9));
    unsigned doy = (153 * mp + 2) / 5 + (unsigned)d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int)doe;
}

static void strip_fractional_seconds(const wchar_t *input, wchar_t out[64]) {
    wcsncpy(out, input ? input : L"", 63);
    out[63] = L'\0';
    wchar_t *dot = wcschr(out, L'.');
    if (dot) {
        *dot = L'\0';
    }
}

void ui_format_request_time_for_list(const wchar_t *dir_name, const wchar_t *fallback, wchar_t out[64]) {
    SYSTEMTIME request_time;
    if (!parse_history_dir_time(dir_name, &request_time)) {
        strip_fractional_seconds(fallback ? fallback : dir_name, out);
        return;
    }

    SYSTEMTIME now;
    GetLocalTime(&now);
    FILETIME request_file_time = { 0 };
    FILETIME now_file_time = { 0 };
    ULARGE_INTEGER request_ticks = { 0 };
    ULARGE_INTEGER now_ticks = { 0 };
    BOOL has_delta = SystemTimeToFileTime(&request_time, &request_file_time) &&
                     SystemTimeToFileTime(&now, &now_file_time);

    if (has_delta) {
        request_ticks.LowPart = request_file_time.dwLowDateTime;
        request_ticks.HighPart = request_file_time.dwHighDateTime;
        now_ticks.LowPart = now_file_time.dwLowDateTime;
        now_ticks.HighPart = now_file_time.dwHighDateTime;
    }

    if (has_delta && now_ticks.QuadPart >= request_ticks.QuadPart) {
        ULONGLONG seconds = (now_ticks.QuadPart - request_ticks.QuadPart) / 10000000ULL;
        if (seconds < 86400ULL) {
            if (seconds < 60ULL) {
                wcscpy(out, L"刚刚");
            } else if (seconds < 3600ULL) {
                swprintf(out, 64, L"%u分钟前", (unsigned)(seconds / 60ULL));
            } else {
                swprintf(out, 64, L"%u小时前", (unsigned)(seconds / 3600ULL));
            }
            return;
        }
    }

    int today = days_from_civil(now.wYear, now.wMonth, now.wDay);
    int request_day = days_from_civil(request_time.wYear, request_time.wMonth, request_time.wDay);
    int day_diff = today - request_day;
    if (day_diff == 1) {
        swprintf(out, 64, L"昨天 %02u:%02u", request_time.wHour, request_time.wMinute);
    } else if (day_diff == 2) {
        swprintf(out, 64, L"前天 %02u:%02u", request_time.wHour, request_time.wMinute);
    } else {
        swprintf(out, 64, L"%04u-%02u-%02u %02u:%02u",
                 request_time.wYear, request_time.wMonth, request_time.wDay,
                 request_time.wHour, request_time.wMinute);
    }
}

int ui_list_insert_record(HWND list, const wchar_t *time, const wchar_t *elapsed, const wchar_t *status,
                          const wchar_t *text, const wchar_t *dir_name) {
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT | LVIF_PARAM;
    item.iItem = 0;
    item.pszText = (LPWSTR)(time ? time : L"");
    item.lParam = (LPARAM)wcs_dup_or_empty(dir_name ? dir_name : L"");
    int index = ListView_InsertItem(list, &item);
    ListView_SetItemText(list, index, 1, (LPWSTR)(elapsed ? elapsed : L"--"));
    ListView_SetItemText(list, index, 2, (LPWSTR)(status ? status : L"--"));
    ListView_SetItemText(list, index, 3, (LPWSTR)(text ? text : L""));
    return index;
}

void ui_list_select_record(HWND list, int index) {
    if (index < 0) {
        return;
    }
    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(list, index, FALSE);
    SetFocus(list);
}

wchar_t *ui_list_get_dir(HWND list, int index) {
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(list, &item)) {
        return (wchar_t *)item.lParam;
    }
    return NULL;
}

void ui_list_set_dir(HWND list, int index, const wchar_t *dir_name) {
    wchar_t *old = ui_list_get_dir(list, index);
    free(old);
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    item.lParam = (LPARAM)wcs_dup_or_empty(dir_name);
    ListView_SetItem(list, &item);
}

int ui_list_find_by_dir(HWND list, const wchar_t *dir_name) {
    int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        wchar_t *item_dir = ui_list_get_dir(list, i);
        if (item_dir && wcscmp(item_dir, dir_name) == 0) {
            return i;
        }
    }
    return -1;
}

void ui_clear_list_lparams(HWND list) {
    int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        wchar_t *dir = ui_list_get_dir(list, i);
        free(dir);
    }
}

void ui_refresh_request_time_column(HWND list) {
    int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        wchar_t *dir = ui_list_get_dir(list, i);
        wchar_t request_time[64];
        ui_format_request_time_for_list(dir, dir, request_time);
        ListView_SetItemText(list, i, 0, request_time);
    }
}

void ui_load_history_into_list(MainState *s) {
    HistoryRecord *records = NULL;
    int count = 0;
    if (!history_load_recent(&records, &count)) {
        return;
    }
    for (int i = count - 1; i >= 0; --i) {
        wchar_t request_time[64];
        ui_format_request_time_for_list(records[i].dir_name, records[i].request_time, request_time);
        ui_list_insert_record(s->list, request_time, records[i].elapsed_text,
                              records[i].status_text, records[i].request_text, records[i].dir_name);
    }
    history_free_records(records, count);
}

void ui_activate_history_record(MainState *s, int index) {
    wchar_t *dir = ui_list_get_dir(s->list, index);
    if (!dir || dir[0] == L'\0') {
        return;
    }
    char *request_json = NULL;
    char *response_json = NULL;
    HistoryRecord meta;
    if (!history_load_detail(dir, &request_json, &response_json, &meta)) {
        MessageBoxW(s->hwnd, L"无法加载历史记录详情。", APP_TITLE, MB_ICONERROR);
        return;
    }
    ui_parse_request_into_controls(s, request_json);
    free_active_response(&s->active);
    s->active.request_time = wcs_dup_or_empty(dir);
    s->active.response_time = wcs_dup_or_empty(meta.response_time);
    s->active.status_code = (DWORD)_wtoi(meta.status_text ? meta.status_text : L"0");
    s->active.elapsed_ms = meta.elapsed_text ? (DWORD)_wtoi(meta.elapsed_text) : 0;
    s->active.audio_duration_ms = -1;
    s->active.prompt_tokens = -1;
    s->active.completion_tokens = -1;
    s->active.total_tokens = -1;
    ParsedAudioResponse parsed;
    wchar_t *parse_error = NULL;
    BOOL ok = audio_parse_response(response_json, meta.output_format, &parsed, &parse_error);
    s->active.preview_text = ok ? parsed.final_text_preview : (parse_error ? parse_error : wcs_dup_or_empty(L"历史响应解析失败。"));
    s->active.raw_response = utf8_to_utf16(response_json ? response_json : "");
    if (ok) {
        s->active.audio.pcm_data = parsed.pcm_data;
        s->active.audio.pcm_size = parsed.pcm_size;
        s->active.audio_duration_ms = parsed.audio_duration_ms;
        s->active.prompt_tokens = parsed.prompt_tokens;
        s->active.completion_tokens = parsed.completion_tokens;
        s->active.total_tokens = parsed.total_tokens;
        parsed.pcm_data = NULL;
        parsed.final_text_preview = NULL;
    }
    wcsncpy(s->active.audio.timestamp, dir, ARRAYSIZE(s->active.audio.timestamp) - 1);
    audio_free_parsed_response(&parsed);
    ui_refresh_response_view(s, ok);
    free(request_json);
    free(response_json);
    free_history_record(&meta);
}
