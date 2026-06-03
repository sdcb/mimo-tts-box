#define WIN32_LEAN_AND_MEAN
#include "ui_main.h"

#include <commctrl.h>
#include <commdlg.h>
#include <process.h>
#include <shellapi.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "config.h"
#include "history_store.h"
#include "http_client.h"
#include "json_helpers.h"
#include "request_builder.h"
#include "resource.h"
#include "win32_helpers.h"

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

static const wchar_t *VOICE_ITEMS[] = { L"mimo_default", L"冰糖", L"茉莉", L"苏打", L"白桦", L"Mia", L"Chloe", L"Milo", L"Dean" };
static const wchar_t *FORMAT_ITEMS[] = { L"wav", L"mp3", L"pcm" };
static const wchar_t *ABOUT_GITHUB_URL = L"https://github.com/sdcb/mimo-tts-box";
static const wchar_t *ABOUT_CJSON_URL = L"https://github.com/DaveGamble/cJSON";

static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK config_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static INT_PTR CALLBACK about_dlg_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK main_child_subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id, DWORD_PTR ref_data);
static void update_multiline_scrollbar(HWND edit);
static void refresh_multiline_scrollbars(MainState *s);
static void on_play(MainState *s);
static void layout_response_status_bar(MainState *s, const DpiMetrics *metrics, int x, int y, int width, int height);
static void set_response_status_bar_text(MainState *s, const wchar_t *status_code, const wchar_t *elapsed,
                                         const wchar_t *duration, const wchar_t *prompt_tokens,
                                         const wchar_t *completion_tokens, const wchar_t *total_tokens);

static MainState *state_from_hwnd(HWND hwnd) {
    return (MainState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
}

static int dpi_from_hwnd(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) {
        ReleaseDC(hwnd, hdc);
    }
    return dpi > 0 ? dpi : 96;
}

static int scale_for_dpi(int value, int dpi) {
    return MulDiv(value, dpi > 0 ? dpi : 96, 96);
}

static DpiMetrics make_dpi_metrics(int dpi) {
    DpiMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.margin = scale_for_dpi(8, dpi);
    metrics.url_h = scale_for_dpi(30, dpi);
    metrics.min_w = scale_for_dpi(640, dpi);
    metrics.min_h = scale_for_dpi(480, dpi);
    metrics.window_w = scale_for_dpi(800, dpi);
    metrics.window_h = scale_for_dpi(600, dpi);
    metrics.send_w = scale_for_dpi(120, dpi);
    metrics.splitter = scale_for_dpi(SPLITTER_HIT_SIZE, dpi);
    metrics.splitter_line = max(1, scale_for_dpi(SPLITTER_LINE_SIZE, dpi));
    metrics.initial_main_split = scale_for_dpi(260, dpi);
    metrics.initial_vertical_split = scale_for_dpi(270, dpi);
    metrics.initial_request_split = scale_for_dpi(260, dpi);
    metrics.main_split_min = scale_for_dpi(180, dpi);
    metrics.main_right_min = scale_for_dpi(300, dpi);
    metrics.split_pane_min = scale_for_dpi(170, dpi);
    metrics.label_h = scale_for_dpi(20, dpi);
    metrics.bottom_controls_h = scale_for_dpi(34, dpi);
    metrics.controls_y_offset = scale_for_dpi(5, dpi);
    metrics.combo_drop_h = scale_for_dpi(400, dpi);
    metrics.voice_w = scale_for_dpi(120, dpi);
    metrics.format_x = scale_for_dpi(130, dpi);
    metrics.format_w = scale_for_dpi(90, dpi);
    metrics.optimize_x = scale_for_dpi(230, dpi);
    metrics.optimize_w = scale_for_dpi(170, dpi);
    metrics.auto_play_x = scale_for_dpi(405, dpi);
    metrics.auto_play_w = scale_for_dpi(115, dpi);
    metrics.check_h = scale_for_dpi(24, dpi);
    metrics.resp_button_w = scale_for_dpi(88, dpi);
    metrics.resp_button_h = scale_for_dpi(28, dpi);
    metrics.resp_button_step = scale_for_dpi(96, dpi);
    metrics.resp_gap = scale_for_dpi(6, dpi);
    metrics.status_h = scale_for_dpi(24, dpi);
    metrics.status_code_w = scale_for_dpi(96, dpi);
    metrics.status_elapsed_w = scale_for_dpi(128, dpi);
    metrics.status_duration_w = scale_for_dpi(128, dpi);
    metrics.status_tokens_min_w = scale_for_dpi(180, dpi);
    return metrics;
}

static HFONT create_ui_font(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int ui_height = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);
    return CreateFontW(ui_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static HWND make_dialog_child(HWND parent, HINSTANCE instance, HFONT font, int dpi, DWORD exstyle,
                              const wchar_t *class_name, const wchar_t *text, DWORD style,
                              int x, int y, int width, int height, int id) {
    HWND child = CreateWindowExW(exstyle, class_name, text, WS_CHILD | WS_VISIBLE | style,
                                 scale_for_dpi(x, dpi), scale_for_dpi(y, dpi),
                                 scale_for_dpi(width, dpi), scale_for_dpi(height, dpi),
                                 parent, (HMENU)(INT_PTR)id, instance, NULL);
    if (child && font) {
        SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
    }
    return child;
}

static void trigger_send_command(MainState *s) {
    if (s) {
        SendMessageW(s->hwnd, WM_COMMAND, MAKEWPARAM(IDC_SEND_BUTTON, BN_CLICKED), (LPARAM)s->send_button);
    }
}

static void open_url(HWND owner, const wchar_t *url) {
    HINSTANCE result = ShellExecuteW(owner, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        MessageBoxW(owner, L"无法打开链接。", APP_TITLE, MB_ICONWARNING);
    }
}

static BOOL is_alt_s_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_SYSCHAR) {
        return wparam == L's' || wparam == L'S';
    }
    if (msg == WM_SYSKEYDOWN) {
        return wparam == 'S' && (lparam & (1 << 29)) != 0;
    }
    return FALSE;
}

static void combo_select_text(HWND combo, const wchar_t *value) {
    int count = (int)SendMessageW(combo, CB_GETCOUNT, 0, 0);
    for (int i = 0; i < count; ++i) {
        wchar_t text[64];
        SendMessageW(combo, CB_GETLBTEXT, (WPARAM)i, (LPARAM)text);
        if (_wcsicmp(text, value ? value : L"") == 0) {
            SendMessageW(combo, CB_SETCURSEL, (WPARAM)i, 0);
            return;
        }
    }
    SendMessageW(combo, CB_SETCURSEL, 0, 0);
}

static wchar_t *combo_get_text(HWND combo) {
    int index = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index < 0) {
        return wcs_dup_or_empty(L"");
    }
    wchar_t text[128];
    SendMessageW(combo, CB_GETLBTEXT, (WPARAM)index, (LPARAM)text);
    return wcs_dup_or_empty(text);
}

static void configure_list_columns(HWND list, int dpi) {
    ListView_SetExtendedListViewStyle(list, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);
    const wchar_t *names[] = { L"请求时间", L"耗时", L"状态码", L"请求文本" };
    int widths[] = { 96, 72, 70, 360 };
    for (int i = 0; i < 4; ++i) {
        LVCOLUMNW col;
        memset(&col, 0, sizeof(col));
        col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        col.pszText = (LPWSTR)names[i];
        col.cx = scale_for_dpi(widths[i], dpi);
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

static void format_request_time_for_list(const wchar_t *dir_name, const wchar_t *fallback, wchar_t out[64]) {
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

static HWND make_child(MainState *s, const wchar_t *class_name, const wchar_t *text, DWORD style, DWORD exstyle, int id) {
    HWND child = CreateWindowExW(exstyle, class_name, text, WS_CHILD | WS_VISIBLE | style,
                                 0, 0, 10, 10, s->hwnd, (HMENU)(INT_PTR)id, s->instance, NULL);
    if (child && s->ui_font) {
        SendMessageW(child, WM_SETFONT, (WPARAM)s->ui_font, TRUE);
    }
    if (child) {
        SetWindowSubclass(child, main_child_subclass_proc, 1, (DWORD_PTR)s);
    }
    return child;
}

static LRESULT paint_label_on_window_background(WPARAM wparam) {
    HDC hdc = (HDC)wparam;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
}

static LRESULT CALLBACK main_child_subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id, DWORD_PTR ref_data) {
    (void)subclass_id;
    MainState *s = (MainState *)ref_data;
    if (is_alt_s_message(msg, wparam, lparam)) {
        trigger_send_command(s);
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, main_child_subclass_proc, 1);
    }
    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

static void populate_request_controls(MainState *s) {
    set_control_text(s->url_edit, s->config->url);
    set_control_text(s->style_edit, s->config->request.style_control);
    set_control_text(s->text_edit, s->config->request.audio_text);
    for (int i = 0; i < (int)ARRAYSIZE(VOICE_ITEMS); ++i) {
        SendMessageW(s->voice_combo, CB_ADDSTRING, 0, (LPARAM)VOICE_ITEMS[i]);
    }
    for (int i = 0; i < (int)ARRAYSIZE(FORMAT_ITEMS); ++i) {
        SendMessageW(s->format_combo, CB_ADDSTRING, 0, (LPARAM)FORMAT_ITEMS[i]);
    }
    combo_select_text(s->voice_combo, s->config->request.voice);
    combo_select_text(s->format_combo, s->config->request.output_format);
    SendMessageW(s->optimize_check, BM_SETCHECK, s->config->request.optimize_text_preview ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(s->auto_play_check, BM_SETCHECK, s->config->request.auto_play_on_download ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(s->stop_button, FALSE);
    refresh_multiline_scrollbars(s);
}

static void update_config_from_controls(MainState *s) {
    wchar_t *url = get_control_text_alloc(s->url_edit);
    wchar_t *style = get_control_text_alloc(s->style_edit);
    wchar_t *text = get_control_text_alloc(s->text_edit);
    wchar_t *voice = combo_get_text(s->voice_combo);
    wchar_t *format = combo_get_text(s->format_combo);
    if (url) {
        free(s->config->url);
        s->config->url = url;
    }
    if (style) {
        free(s->config->request.style_control);
        s->config->request.style_control = style;
    }
    if (text) {
        free(s->config->request.audio_text);
        s->config->request.audio_text = text;
    }
    if (voice) {
        free(s->config->request.voice);
        s->config->request.voice = voice;
    }
    if (format) {
        free(s->config->request.output_format);
        s->config->request.output_format = format;
    }
    s->config->request.optimize_text_preview = SendMessageW(s->optimize_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
    s->config->request.auto_play_on_download = SendMessageW(s->auto_play_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static int list_insert_record(HWND list, const wchar_t *time, const wchar_t *elapsed, const wchar_t *status, const wchar_t *text, const wchar_t *dir_name) {
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

static void list_select_record(HWND list, int index) {
    if (index < 0) {
        return;
    }
    ListView_SetItemState(list, -1, 0, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_SetItemState(list, index, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    ListView_EnsureVisible(list, index, FALSE);
    SetFocus(list);
}

static wchar_t *list_get_dir(HWND list, int index) {
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    if (ListView_GetItem(list, &item)) {
        return (wchar_t *)item.lParam;
    }
    return NULL;
}

static void list_set_dir(HWND list, int index, const wchar_t *dir_name) {
    wchar_t *old = list_get_dir(list, index);
    free(old);
    LVITEMW item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_PARAM;
    item.iItem = index;
    item.lParam = (LPARAM)wcs_dup_or_empty(dir_name);
    ListView_SetItem(list, &item);
}

static int list_find_by_dir(HWND list, const wchar_t *dir_name) {
    int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        wchar_t *item_dir = list_get_dir(list, i);
        if (item_dir && wcscmp(item_dir, dir_name) == 0) {
            return i;
        }
    }
    return -1;
}

static void clear_list_lparams(HWND list) {
    int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        wchar_t *dir = list_get_dir(list, i);
        free(dir);
    }
}

static void refresh_request_time_column(HWND list) {
    int count = ListView_GetItemCount(list);
    for (int i = 0; i < count; ++i) {
        wchar_t *dir = list_get_dir(list, i);
        wchar_t request_time[64];
        format_request_time_for_list(dir, dir, request_time);
        ListView_SetItemText(list, i, 0, request_time);
    }
}

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

static void load_history_into_list(MainState *s) {
    HistoryRecord *records = NULL;
    int count = 0;
    if (!history_load_recent(&records, &count)) {
        return;
    }
    for (int i = count - 1; i >= 0; --i) {
        wchar_t request_time[64];
        format_request_time_for_list(records[i].dir_name, records[i].request_time, request_time);
        list_insert_record(s->list, request_time, records[i].elapsed_text,
                           records[i].status_text, records[i].request_text, records[i].dir_name);
    }
    history_free_records(records, count);
}

static void set_active_from_result(MainState *s, RequestResult *result) {
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

static void set_response_status_bar_text(MainState *s, const wchar_t *status_code, const wchar_t *elapsed,
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

static void refresh_response_view(MainState *s, BOOL success_like) {
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
    set_response_status_bar_text(s, status_code, s->active.elapsed_ms > 0 ? elapsed : L"--", duration,
                                 prompt_tokens, completion_tokens, total_tokens);
    set_control_text(s->preview_edit, success_like ? s->active.preview_text : (s->active.raw_response ? s->active.raw_response : s->active.preview_text));
    SendMessageW(s->preview_edit, WM_SETFONT, (WPARAM)(success_like ? s->ui_font : s->mono_font), TRUE);
    update_multiline_scrollbar(s->preview_edit);
    EnableWindow(s->play_button, s->active.audio.pcm_data && s->active.audio.pcm_size > 0);
    EnableWindow(s->save_button, s->active.audio.pcm_data && s->active.audio.pcm_size > 0);
    EnableWindow(s->stop_button, FALSE);
}

static void update_multiline_scrollbar(HWND edit) {
    if (!edit) {
        return;
    }
    RECT rc;
    GetClientRect(edit, &rc);
    int client_h = rc.bottom - rc.top;
    int client_w = rc.right - rc.left;
    if (client_h <= 0 || client_w <= 0) {
        ShowScrollBar(edit, SB_VERT, FALSE);
        return;
    }

    HDC hdc = GetDC(edit);
    HFONT font = (HFONT)SendMessageW(edit, WM_GETFONT, 0, 0);
    HGDIOBJ old_font = font ? SelectObject(hdc, font) : NULL;
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int line_h = tm.tmHeight + tm.tmExternalLeading;
    if (line_h <= 0) {
        line_h = 16;
    }
    int visible_lines = client_h / line_h;
    int available_w = client_w - GetSystemMetrics(SM_CXVSCROLL) - 8;
    if (available_w < 1) {
        available_w = client_w;
    }

    int visual_lines = 1;
    wchar_t *text = get_control_text_alloc(edit);
    if (text) {
        visual_lines = 0;
        const wchar_t *line_start = text;
        const wchar_t *p = text;
        while (TRUE) {
            if (*p == L'\r' || *p == L'\n' || *p == L'\0') {
                int len = (int)(p - line_start);
                SIZE size = { 0 };
                if (len > 0) {
                    GetTextExtentPoint32W(hdc, line_start, len, &size);
                    visual_lines += (size.cx + available_w - 1) / available_w;
                } else {
                    visual_lines += 1;
                }
                if (*p == L'\0') {
                    break;
                }
                if (*p == L'\r' && p[1] == L'\n') {
                    ++p;
                }
                line_start = p + 1;
            }
            ++p;
        }
        free(text);
    }
    if (old_font) {
        SelectObject(hdc, old_font);
    }
    ReleaseDC(edit, hdc);

    int line_count = (int)SendMessageW(edit, EM_GETLINECOUNT, 0, 0);
    if (line_count > visual_lines) {
        visual_lines = line_count;
    }
    BOOL need_scroll = visual_lines > visible_lines;
    LONG_PTR style = GetWindowLongPtrW(edit, GWL_STYLE);
    BOOL has_scroll = (style & WS_VSCROLL) != 0;
    if (need_scroll != has_scroll) {
        if (need_scroll) {
            style |= WS_VSCROLL;
        } else {
            style &= ~WS_VSCROLL;
        }
        SetWindowLongPtrW(edit, GWL_STYLE, style);
        SetWindowPos(edit, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    ShowScrollBar(edit, SB_VERT, need_scroll);
}

static void refresh_multiline_scrollbars(MainState *s) {
    update_multiline_scrollbar(s->style_edit);
    update_multiline_scrollbar(s->text_edit);
    update_multiline_scrollbar(s->preview_edit);
}

static void parse_request_into_controls(MainState *s, const char *request_json) {
    cJSON *root = cJSON_Parse(request_json);
    if (!root) {
        return;
    }
    const cJSON *messages = cJSON_GetObjectItemCaseSensitive(root, "messages");
    const cJSON *style_msg = cJSON_IsArray(messages) ? cJSON_GetArrayItem(messages, 0) : NULL;
    const cJSON *text_msg = cJSON_IsArray(messages) ? cJSON_GetArrayItem(messages, 1) : NULL;
    const cJSON *audio = json_get_object(root, "audio");
    const char *style = style_msg ? json_get_string_value(style_msg, "content") : NULL;
    const char *text = text_msg ? json_get_string_value(text_msg, "content") : NULL;
    const char *voice = audio ? json_get_string_value(audio, "voice") : NULL;
    const char *format = audio ? json_get_string_value(audio, "format") : NULL;
    if (style) {
        wchar_t *wide = utf8_to_utf16(style);
        set_control_text(s->style_edit, wide);
        free(wide);
    }
    if (text) {
        wchar_t *wide = utf8_to_utf16(text);
        set_control_text(s->text_edit, wide);
        free(wide);
    }
    if (voice) {
        wchar_t *wide = utf8_to_utf16(voice);
        combo_select_text(s->voice_combo, wide);
        free(wide);
    }
    if (format) {
        wchar_t *wide = utf8_to_utf16(format);
        combo_select_text(s->format_combo, wide);
        free(wide);
    }
    if (audio) {
        SendMessageW(s->optimize_check, BM_SETCHECK,
                     json_get_bool_value(audio, "optimize_text_preview", TRUE) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    refresh_multiline_scrollbars(s);
    cJSON_Delete(root);
}

static void activate_history_record(MainState *s, int index) {
    wchar_t *dir = list_get_dir(s->list, index);
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
    parse_request_into_controls(s, request_json);
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
    refresh_response_view(s, ok);
    free(request_json);
    free(response_json);
    free_history_record(&meta);
}

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
    PostMessageW(task->hwnd, WM_APP_REQUEST_DONE, 0, (LPARAM)result);
    free_request_task(task);
}

static BOOL show_config_dialog(HWND owner, AppConfig *config) {
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = config_wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MimoTTSBoxConfigWindow";
    RegisterClassW(&wc);
    ConfigDialogState state;
    memset(&state, 0, sizeof(state));
    state.config = config;
    state.dpi = dpi_from_hwnd(owner);
    RECT owner_rect;
    GetWindowRect(owner, &owner_rect);
    int width = scale_for_dpi(520, state.dpi);
    int height = scale_for_dpi(190, state.dpi);
    int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME, wc.lpszClassName, L"配置",
                               WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, width, height,
                               owner, NULL, instance, &state);
    if (!dlg) {
        return FALSE;
    }
    EnableWindow(owner, FALSE);
    ShowWindow(dlg, SW_SHOW);
    MSG msg;
    while (!state.done && GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    EnableWindow(owner, TRUE);
    SetActiveWindow(owner);
    return state.accepted;
}

static void show_about_dialog(HWND owner) {
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), owner, about_dlg_proc, 0);
}

static void on_send(MainState *s) {
    update_config_from_controls(s);
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
        if (!show_config_dialog(s->hwnd, s->config) || !config_has_api_key(s->config)) {
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
    format_request_time_for_list(dir, display_time, list_time);
    LONG request_id = InterlockedIncrement(&s->next_request_id);
    s->latest_request_id = request_id;
    int index = list_insert_record(s->list, list_time, L"--", L"--", s->config->request.audio_text, dir);
    list_select_record(s->list, index);

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

static void on_request_done(MainState *s, RequestResult *result) {
    wchar_t elapsed[32];
    wchar_t status[32];
    swprintf(elapsed, ARRAYSIZE(elapsed), L"%lums", (unsigned long)result->elapsed_ms);
    if (result->status_code > 0) {
        swprintf(status, ARRAYSIZE(status), L"%lu", (unsigned long)result->status_code);
    } else {
        wcscpy(status, L"--");
    }
    int index = list_find_by_dir(s->list, result->history_dir);
    if (index < 0) {
        index = result->list_index;
    }
    ListView_SetItemText(s->list, index, 1, elapsed);
    ListView_SetItemText(s->list, index, 2, status);
    list_set_dir(s->list, index, result->history_dir);
    if (result->request_id == s->latest_request_id) {
        set_active_from_result(s, result);
        refresh_response_view(s, result->success);
        if (result->success && result->auto_play_on_download && s->active.audio.pcm_data && s->active.audio.pcm_size > 0) {
            on_play(s);
        }
    }
    free_request_result(result);
}

static void on_play(MainState *s) {
    wchar_t *error = NULL;
    if (!audio_play(&s->active.audio, &error)) {
        MessageBoxW(s->hwnd, error ? error : L"播放失败。", APP_TITLE, MB_ICONERROR);
        free(error);
        return;
    }
    EnableWindow(s->play_button, FALSE);
    EnableWindow(s->stop_button, TRUE);
}

static void on_save(MainState *s) {
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

static void layout_controls(MainState *s) {
    DpiMetrics metrics = make_dpi_metrics(s->dpi);
    RECT rc;
    GetClientRect(s->hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int bottom_y = metrics.margin + metrics.url_h + metrics.margin;
    if (w < metrics.min_w) {
        w = metrics.min_w;
    }
    if (h < metrics.min_h) {
        h = metrics.min_h;
    }
    MoveWindow(s->url_edit, metrics.margin, metrics.margin, w - metrics.send_w - metrics.margin * 3,
               metrics.url_h, TRUE);
    MoveWindow(s->send_button, w - metrics.send_w - metrics.margin, metrics.margin, metrics.send_w,
               metrics.url_h, TRUE);

    int main_top = bottom_y;
    int main_h = h - main_top - metrics.margin;
    if (s->main_split < metrics.main_split_min) {
        s->main_split = metrics.main_split_min;
    }
    if (s->main_split > w - metrics.main_right_min) {
        s->main_split = w - metrics.main_right_min;
    }
    int left_w = s->main_split;
    int right_x = metrics.margin + left_w + metrics.splitter;
    int right_w = w - right_x - metrics.margin;
    MoveWindow(s->list, metrics.margin, main_top, left_w, main_h, TRUE);

    if (s->vertical_split < metrics.split_pane_min) {
        s->vertical_split = metrics.split_pane_min;
    }
    if (s->vertical_split > main_h - metrics.split_pane_min) {
        s->vertical_split = main_h - metrics.split_pane_min;
    }
    int req_h = s->vertical_split;
    int resp_y = main_top + req_h + metrics.splitter;
    int resp_h = main_h - req_h - metrics.splitter;
    if (s->request_split < metrics.split_pane_min) {
        s->request_split = metrics.split_pane_min;
    }
    if (s->request_split > right_w - metrics.split_pane_min) {
        s->request_split = right_w - metrics.split_pane_min;
    }
    int style_w = s->request_split;
    int text_x = right_x + style_w + metrics.splitter;
    int text_w = right_w - style_w - metrics.splitter;
    MoveWindow(s->style_label, right_x, main_top, style_w, metrics.label_h, TRUE);
    MoveWindow(s->text_label, text_x, main_top, text_w, metrics.label_h, TRUE);
    MoveWindow(s->style_edit, right_x, main_top + metrics.label_h, style_w,
               req_h - metrics.label_h - metrics.bottom_controls_h, TRUE);
    MoveWindow(s->text_edit, text_x, main_top + metrics.label_h, text_w,
               req_h - metrics.label_h - metrics.bottom_controls_h, TRUE);
    int controls_y = main_top + req_h - metrics.bottom_controls_h + metrics.controls_y_offset;
    MoveWindow(s->voice_combo, right_x, controls_y, metrics.voice_w, metrics.combo_drop_h, TRUE);
    MoveWindow(s->format_combo, right_x + metrics.format_x, controls_y, metrics.format_w,
               metrics.combo_drop_h, TRUE);
    MoveWindow(s->optimize_check, right_x + metrics.optimize_x, controls_y, metrics.optimize_w,
               metrics.check_h, TRUE);
    MoveWindow(s->auto_play_check, right_x + metrics.auto_play_x, controls_y, metrics.auto_play_w,
               metrics.check_h, TRUE);

    MoveWindow(s->play_button, right_x, resp_y, metrics.resp_button_w, metrics.resp_button_h, TRUE);
    MoveWindow(s->stop_button, right_x + metrics.resp_button_step, resp_y, metrics.resp_button_w,
               metrics.resp_button_h, TRUE);
    MoveWindow(s->save_button, right_x + metrics.resp_button_step * 2, resp_y, metrics.resp_button_w,
               metrics.resp_button_h, TRUE);
    MoveWindow(s->preview_edit, right_x, resp_y + metrics.resp_button_h + metrics.resp_gap, right_w,
               resp_h - metrics.resp_button_h - metrics.resp_gap - metrics.status_h - metrics.resp_gap, TRUE);
    layout_response_status_bar(s, &metrics, right_x, resp_y + resp_h - metrics.status_h, right_w,
                               metrics.status_h);
    refresh_multiline_scrollbars(s);
    InvalidateRect(s->hwnd, NULL, TRUE);
}

static void layout_response_status_bar(MainState *s, const DpiMetrics *metrics, int x, int y, int width, int height) {
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

static BOOL hit_rect_point(RECT rc, int x, int y) {
    return x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom;
}

static DragMode hit_test_splitter(MainState *s, int x, int y) {
    DpiMetrics metrics = make_dpi_metrics(s->dpi);
    RECT rc;
    GetClientRect(s->hwnd, &rc);
    int main_top = metrics.margin + metrics.url_h + metrics.margin;
    int main_h = rc.bottom - main_top - metrics.margin;
    RECT main_sp = { metrics.margin + s->main_split, main_top,
                     metrics.margin + s->main_split + metrics.splitter, main_top + main_h };
    int right_x = metrics.margin + s->main_split + metrics.splitter;
    int right_w = rc.right - right_x - metrics.margin;
    RECT vertical_sp = { right_x, main_top + s->vertical_split, right_x + right_w,
                         main_top + s->vertical_split + metrics.splitter };
    RECT request_sp = { right_x + s->request_split, main_top, right_x + s->request_split + metrics.splitter,
                        main_top + s->vertical_split };
    if (hit_rect_point(main_sp, x, y)) {
        return DRAG_MAIN;
    }
    if (hit_rect_point(vertical_sp, x, y)) {
        return DRAG_VERTICAL;
    }
    if (hit_rect_point(request_sp, x, y)) {
        return DRAG_REQUEST;
    }
    return DRAG_NONE;
}

static void paint_splitters(MainState *s, HDC hdc) {
    DpiMetrics metrics = make_dpi_metrics(s->dpi);
    RECT rc;
    GetClientRect(s->hwnd, &rc);
    int main_top = metrics.margin + metrics.url_h + metrics.margin;
    int main_h = rc.bottom - main_top - metrics.margin;
    HBRUSH brush = CreateSolidBrush(RGB(224, 228, 233));
    int main_line_x = metrics.margin + s->main_split + (metrics.splitter - metrics.splitter_line) / 2;
    RECT r1 = { main_line_x, main_top, main_line_x + metrics.splitter_line, main_top + main_h };
    FillRect(hdc, &r1, brush);
    int right_x = metrics.margin + s->main_split + metrics.splitter;
    int right_w = rc.right - right_x - metrics.margin;
    int vertical_line_y = main_top + s->vertical_split + (metrics.splitter - metrics.splitter_line) / 2;
    RECT r2 = { right_x, vertical_line_y, right_x + right_w, vertical_line_y + metrics.splitter_line };
    FillRect(hdc, &r2, brush);
    int request_line_x = right_x + s->request_split + (metrics.splitter - metrics.splitter_line) / 2;
    RECT r3 = { request_line_x, main_top, request_line_x + metrics.splitter_line, main_top + s->vertical_split };
    FillRect(hdc, &r3, brush);
    DeleteObject(brush);
}

static void create_main_menu(HWND hwnd) {
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    HMENU resource_menu = LoadMenuW(instance, L"MAINMENU");
    if (resource_menu) {
        SetMenu(hwnd, resource_menu);
        return;
    }
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU help = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_FILE_CONFIG, L"配置(&C)");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, IDM_FILE_EXIT, L"退出(&X)");
    AppendMenuW(help, MF_STRING, IDM_HELP_ABOUT, L"关于(&A)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)file, L"文件(&F)");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)help, L"帮助(&H)");
    SetMenu(hwnd, menu);
}

static BOOL on_create(HWND hwnd, CREATESTRUCTW *cs) {
    MainState *s = (MainState *)calloc(1, sizeof(MainState));
    if (!s) {
        return FALSE;
    }
    s->instance = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
    s->hwnd = hwnd;
    s->config = (AppConfig *)cs->lpCreateParams;
    s->dpi = dpi_from_hwnd(hwnd);
    DpiMetrics metrics = make_dpi_metrics(s->dpi);
    s->main_split = metrics.initial_main_split;
    s->vertical_split = metrics.initial_vertical_split;
    s->request_split = metrics.initial_request_split;
    HDC hdc = GetDC(hwnd);
    int ui_height = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);
    s->ui_font = create_ui_font(hwnd);
    s->emoji_font = CreateFontW(ui_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Emoji");
    s->mono_font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
    create_main_menu(hwnd);
    s->url_edit = make_child(s, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_URL_EDIT);
    s->send_button = make_child(s, L"BUTTON", L"发送(&S)", BS_PUSHBUTTON, 0, IDC_SEND_BUTTON);
    s->list = make_child(s, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_HISTORY_LIST);
    configure_list_columns(s->list, s->dpi);
    s->style_label = make_child(s, L"STATIC", L"风格控制", 0, 0, 0);
    s->text_label = make_child(s, L"STATIC", L"音频合成文本", 0, 0, 0);
    s->style_edit = make_child(s, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, IDC_STYLE_EDIT);
    s->text_edit = make_child(s, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, IDC_TEXT_EDIT);
    s->voice_combo = make_child(s, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 0, IDC_VOICE_COMBO);
    s->format_combo = make_child(s, L"COMBOBOX", L"", CBS_DROPDOWNLIST, 0, IDC_FORMAT_COMBO);
    s->optimize_check = make_child(s, L"BUTTON", L"optimize_text_preview", BS_AUTOCHECKBOX, 0, IDC_OPTIMIZE_CHECK);
    s->auto_play_check = make_child(s, L"BUTTON", L"下载后播放", BS_AUTOCHECKBOX, 0, IDC_AUTO_PLAY_CHECK);
    s->status_bar = make_child(s, STATUSCLASSNAMEW, L"", CCS_NOPARENTALIGN | CCS_NORESIZE | SBARS_TOOLTIPS, 0, IDC_STATUS_BAR);
    s->preview_edit = make_child(s, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, WS_EX_CLIENTEDGE, IDC_PREVIEW_EDIT);
    s->play_button = make_child(s, L"BUTTON", L"\x25b6\xfe0f 播放", BS_PUSHBUTTON, 0, IDC_PLAY_BUTTON);
    s->stop_button = make_child(s, L"BUTTON", L"\x23f9\xfe0f 停止", BS_PUSHBUTTON, 0, IDC_STOP_BUTTON);
    s->save_button = make_child(s, L"BUTTON", L"\xd83d\xddc3\xfe0f 保存", BS_PUSHBUTTON, 0, IDC_SAVE_BUTTON);
    SendMessageW(s->play_button, WM_SETFONT, (WPARAM)s->emoji_font, TRUE);
    SendMessageW(s->stop_button, WM_SETFONT, (WPARAM)s->emoji_font, TRUE);
    SendMessageW(s->save_button, WM_SETFONT, (WPARAM)s->emoji_font, TRUE);
    populate_request_controls(s);
    load_history_into_list(s);
    wchar_t *audio_error = NULL;
    if (!audio_system_init(hwnd, &audio_error) && audio_error) {
        MessageBoxW(hwnd, audio_error, APP_TITLE, MB_ICONWARNING);
        free(audio_error);
    }
    layout_controls(s);
    set_response_status_bar_text(s, L"--", L"--", L"--", L"--", L"--", L"--");
    SetTimer(hwnd, REQUEST_TIME_TIMER_ID, REQUEST_TIME_REFRESH_MS, NULL);
    return TRUE;
}

BOOL ui_register_main_window(HINSTANCE instance) {
    WNDCLASSW wc;
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = main_wnd_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APPICON));
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"MimoTTSBoxMainWindow";
    return RegisterClassW(&wc) != 0;
}

HWND ui_create_main_window(HINSTANCE instance, AppConfig *config) {
    int dpi = dpi_from_hwnd(NULL);
    DpiMetrics metrics = make_dpi_metrics(dpi);
    return CreateWindowExW(0, L"MimoTTSBoxMainWindow", APP_TITLE,
                           WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CW_USEDEFAULT, CW_USEDEFAULT, metrics.window_w, metrics.window_h,
                           NULL, NULL, instance, config);
}

static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    MainState *s = state_from_hwnd(hwnd);
    switch (msg) {
    case WM_CREATE:
        return on_create(hwnd, (CREATESTRUCTW *)lparam) ? 0 : -1;
    case WM_SIZE:
        if (s) {
            layout_controls(s);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)lparam;
        int dpi = s ? s->dpi : 96;
        DpiMetrics metrics = make_dpi_metrics(dpi);
        info->ptMinTrackSize.x = metrics.min_w;
        info->ptMinTrackSize.y = metrics.min_h;
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HWND child = (HWND)lparam;
        if (s && (child == s->style_label || child == s->text_label ||
                                    child == s->optimize_check || child == s->auto_play_check)) {
            return paint_label_on_window_background(wparam);
        }
        break;
    }
    case WM_SYSCHAR:
    case WM_SYSKEYDOWN:
        if (s && is_alt_s_message(msg, wparam, lparam)) {
            trigger_send_command(s);
            return 0;
        }
        break;
    case WM_COMMAND:
        if (!s) {
            break;
        }
        if (HIWORD(wparam) == EN_CHANGE &&
            (LOWORD(wparam) == IDC_STYLE_EDIT || LOWORD(wparam) == IDC_TEXT_EDIT ||
             LOWORD(wparam) == IDC_PREVIEW_EDIT)) {
            update_multiline_scrollbar((HWND)lparam);
            return 0;
        }
        switch (LOWORD(wparam)) {
        case IDC_SEND_BUTTON:
            on_send(s);
            return 0;
        case IDC_PLAY_BUTTON:
            on_play(s);
            return 0;
        case IDC_STOP_BUTTON:
            audio_stop();
            EnableWindow(s->play_button, TRUE);
            EnableWindow(s->stop_button, FALSE);
            return 0;
        case IDC_SAVE_BUTTON:
            on_save(s);
            return 0;
        case IDM_FILE_CONFIG:
            update_config_from_controls(s);
            if (show_config_dialog(hwnd, s->config)) {
                set_control_text(s->url_edit, s->config->url);
                config_save(s->config, NULL);
            }
            return 0;
        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case IDM_HELP_ABOUT:
            show_about_dialog(hwnd);
            return 0;
        }
        break;
    case WM_NOTIFY:
        if (s && ((LPNMHDR)lparam)->idFrom == IDC_HISTORY_LIST && ((LPNMHDR)lparam)->code == NM_DBLCLK) {
            LPNMITEMACTIVATE act = (LPNMITEMACTIVATE)lparam;
            if (act->iItem >= 0) {
                activate_history_record(s, act->iItem);
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if (s && wparam == REQUEST_TIME_TIMER_ID) {
            refresh_request_time_column(s->list);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (s) {
            int x = GET_X_LPARAM(lparam);
            int y = GET_Y_LPARAM(lparam);
            s->drag = hit_test_splitter(s, x, y);
            if (s->drag != DRAG_NONE) {
                SetCapture(hwnd);
                return 0;
            }
        }
        break;
    case WM_MOUSEMOVE:
        if (s) {
            int x = GET_X_LPARAM(lparam);
            int y = GET_Y_LPARAM(lparam);
            if (s->drag == DRAG_NONE) {
                DragMode hit = hit_test_splitter(s, x, y);
                SetCursor(LoadCursor(NULL, hit == DRAG_VERTICAL ? IDC_SIZENS : (hit == DRAG_NONE ? IDC_ARROW : IDC_SIZEWE)));
            } else {
                DpiMetrics metrics = make_dpi_metrics(s->dpi);
                int main_top = metrics.margin + metrics.url_h + metrics.margin;
                int right_x = metrics.margin + s->main_split + metrics.splitter;
                if (s->drag == DRAG_MAIN) {
                    s->main_split = x - metrics.margin;
                } else if (s->drag == DRAG_VERTICAL) {
                    s->vertical_split = y - main_top;
                } else if (s->drag == DRAG_REQUEST) {
                    s->request_split = x - right_x;
                }
                layout_controls(s);
                return 0;
            }
        }
        break;
    case WM_LBUTTONUP:
        if (s && s->drag != DRAG_NONE) {
            s->drag = DRAG_NONE;
            ReleaseCapture();
            return 0;
        }
        break;
    case WM_PAINT:
        if (s) {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paint_splitters(s, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    case WM_APP_REQUEST_DONE:
        if (s) {
            on_request_done(s, (RequestResult *)lparam);
        } else {
            free_request_result((RequestResult *)lparam);
        }
        return 0;
    case WM_APP_PLAYBACK_DONE:
        if (s) {
            audio_stop();
            EnableWindow(s->play_button, s->active.audio.pcm_data && s->active.audio.pcm_size > 0);
            EnableWindow(s->stop_button, FALSE);
        }
        return 0;
    case WM_DESTROY:
        if (s) {
            KillTimer(hwnd, REQUEST_TIME_TIMER_ID);
            clear_list_lparams(s->list);
            free_active_response(&s->active);
            if (s->ui_font) {
                DeleteObject(s->ui_font);
            }
            if (s->emoji_font) {
                DeleteObject(s->emoji_font);
            }
            free(s);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void close_config_dialog(ConfigDialogState *s, BOOL accepted) {
    if (accepted) {
        wchar_t *url = get_control_text_alloc(s->url_edit);
        wchar_t *api = get_control_text_alloc(s->api_edit);
        if (!url || url[0] == L'\0') {
            MessageBoxW(s->hwnd, L"URL 不能为空。", APP_TITLE, MB_ICONWARNING);
            free(url);
            free(api);
            return;
        }
        free(s->config->url);
        s->config->url = url;
        free(s->config->api_key);
        s->config->api_key = api ? api : wcs_dup_or_empty(L"");
        wchar_t *error = NULL;
        if (!config_save(s->config, &error)) {
            MessageBoxW(s->hwnd, error ? error : L"保存配置失败。", APP_TITLE, MB_ICONERROR);
            free(error);
            return;
        }
        s->accepted = TRUE;
    }
    s->done = TRUE;
    DestroyWindow(s->hwnd);
}

static LRESULT CALLBACK config_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    ConfigDialogState *s = (ConfigDialogState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_CREATE: {
        CREATESTRUCTW *cs = (CREATESTRUCTW *)lparam;
        s = (ConfigDialogState *)cs->lpCreateParams;
        s->hwnd = hwnd;
        s->ui_font = create_ui_font(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"STATIC", L"Url", 0,
                          14, 20, 70, 24, 0);
        s->url_edit = make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, WS_EX_CLIENTEDGE,
                                        L"EDIT", s->config->url, ES_AUTOHSCROLL,
                                        90, 18, 400, 24, 0);
        make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"STATIC", L"API Key", 0,
                          14, 58, 70, 24, 0);
        s->api_edit = make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, WS_EX_CLIENTEDGE,
                                        L"EDIT", s->config->api_key, ES_PASSWORD | ES_AUTOHSCROLL,
                                        90, 56, 400, 24, 0);
        make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"BUTTON", L"确定", BS_DEFPUSHBUTTON,
                          318, 112, 80, 28, IDOK);
        make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"BUTTON", L"取消", BS_PUSHBUTTON,
                          410, 112, 80, 28, IDCANCEL);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK) {
            close_config_dialog(s, TRUE);
            return 0;
        }
        if (LOWORD(wparam) == IDCANCEL) {
            close_config_dialog(s, FALSE);
            return 0;
        }
        break;
    case WM_CTLCOLORSTATIC:
        return paint_label_on_window_background(wparam);
    case WM_DESTROY:
        if (s && s->ui_font) {
            DeleteObject(s->ui_font);
            s->ui_font = NULL;
        }
        return 0;
    case WM_CLOSE:
        close_config_dialog(s, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static void center_dialog_over_owner(HWND hwnd) {
    HWND owner = GetParent(hwnd);
    if (!owner) {
        return;
    }
    RECT owner_rect;
    RECT dialog_rect;
    GetWindowRect(owner, &owner_rect);
    GetWindowRect(hwnd, &dialog_rect);

    int width = dialog_rect.right - dialog_rect.left;
    int height = dialog_rect.bottom - dialog_rect.top;
    int x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    int y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE | SWP_NOACTIVATE);
}

static INT_PTR CALLBACK about_dlg_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    (void)lparam;
    switch (msg) {
    case WM_INITDIALOG:
        center_dialog_over_owner(hwnd);
        return TRUE;
    case WM_COMMAND:
        if (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL) {
            EndDialog(hwnd, LOWORD(wparam));
            return TRUE;
        }
        break;
    case WM_NOTIFY: {
        LPNMHDR hdr = (LPNMHDR)lparam;
        if (hdr->code == NM_CLICK || hdr->code == NM_RETURN) {
            if (hdr->idFrom == IDC_ABOUT_GITHUB_LINK) {
                open_url(hwnd, ABOUT_GITHUB_URL);
                return TRUE;
            }
            if (hdr->idFrom == IDC_ABOUT_CJSON_LINK) {
                open_url(hwnd, ABOUT_CJSON_URL);
                return TRUE;
            }
        }
        break;
    }
    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
