#define WIN32_LEAN_AND_MEAN
#include "ui_controls.h"

#include <commctrl.h>
#include <stdlib.h>

#include "json_helpers.h"
#include "ui_metrics.h"
#include "win32_helpers.h"

static const wchar_t *VOICE_ITEMS[] = { L"mimo_default", L"冰糖", L"茉莉", L"苏打", L"白桦", L"Mia", L"Chloe", L"Milo", L"Dean" };
static const wchar_t *FORMAT_ITEMS[] = { L"wav", L"mp3", L"pcm" };

static LRESULT CALLBACK main_child_subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id, DWORD_PTR ref_data);

HWND ui_make_child(MainState *s, const wchar_t *class_name, const wchar_t *text, DWORD style, DWORD exstyle, int id) {
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

HWND ui_make_dialog_child(HWND parent, HINSTANCE instance, HFONT font, int dpi, DWORD exstyle,
                          const wchar_t *class_name, const wchar_t *text, DWORD style,
                          int x, int y, int width, int height, int id) {
    HWND child = CreateWindowExW(exstyle, class_name, text, WS_CHILD | WS_VISIBLE | style,
                                 ui_scale_for_dpi(x, dpi), ui_scale_for_dpi(y, dpi),
                                 ui_scale_for_dpi(width, dpi), ui_scale_for_dpi(height, dpi),
                                 parent, (HMENU)(INT_PTR)id, instance, NULL);
    if (child && font) {
        SendMessageW(child, WM_SETFONT, (WPARAM)font, TRUE);
    }
    return child;
}

void ui_trigger_send_command(MainState *s) {
    if (s) {
        SendMessageW(s->hwnd, WM_COMMAND, MAKEWPARAM(IDC_SEND_BUTTON, BN_CLICKED), (LPARAM)s->send_button);
    }
}

BOOL ui_is_alt_s_message(UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_SYSCHAR) {
        return wparam == L's' || wparam == L'S';
    }
    if (msg == WM_SYSKEYDOWN) {
        return wparam == 'S' && (lparam & (1 << 29)) != 0;
    }
    return FALSE;
}

void ui_combo_select_text(HWND combo, const wchar_t *value) {
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

wchar_t *ui_combo_get_text(HWND combo) {
    int index = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
    if (index < 0) {
        return wcs_dup_or_empty(L"");
    }
    wchar_t text[128];
    SendMessageW(combo, CB_GETLBTEXT, (WPARAM)index, (LPARAM)text);
    return wcs_dup_or_empty(text);
}

LRESULT ui_paint_label_on_window_background(WPARAM wparam) {
    HDC hdc = (HDC)wparam;
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
    return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
}

static LRESULT CALLBACK main_child_subclass_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam,
                                                 UINT_PTR subclass_id, DWORD_PTR ref_data) {
    (void)subclass_id;
    MainState *s = (MainState *)ref_data;
    if (ui_is_alt_s_message(msg, wparam, lparam)) {
        ui_trigger_send_command(s);
        return 0;
    }
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, main_child_subclass_proc, 1);
    }
    return DefSubclassProc(hwnd, msg, wparam, lparam);
}

void ui_populate_request_controls(MainState *s) {
    set_control_text(s->url_edit, s->config->url);
    set_control_text(s->style_edit, s->config->request.style_control);
    set_control_text(s->text_edit, s->config->request.audio_text);
    for (int i = 0; i < (int)ARRAYSIZE(VOICE_ITEMS); ++i) {
        SendMessageW(s->voice_combo, CB_ADDSTRING, 0, (LPARAM)VOICE_ITEMS[i]);
    }
    for (int i = 0; i < (int)ARRAYSIZE(FORMAT_ITEMS); ++i) {
        SendMessageW(s->format_combo, CB_ADDSTRING, 0, (LPARAM)FORMAT_ITEMS[i]);
    }
    ui_combo_select_text(s->voice_combo, s->config->request.voice);
    ui_combo_select_text(s->format_combo, s->config->request.output_format);
    SendMessageW(s->optimize_check, BM_SETCHECK, s->config->request.optimize_text_preview ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(s->auto_play_check, BM_SETCHECK, s->config->request.auto_play_on_download ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(s->stop_button, FALSE);
    ui_refresh_multiline_scrollbars(s);
}

void ui_update_config_from_controls(MainState *s) {
    wchar_t *url = get_control_text_alloc(s->url_edit);
    wchar_t *style = get_control_text_alloc(s->style_edit);
    wchar_t *text = get_control_text_alloc(s->text_edit);
    wchar_t *voice = ui_combo_get_text(s->voice_combo);
    wchar_t *format = ui_combo_get_text(s->format_combo);
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

void ui_parse_request_into_controls(MainState *s, const char *request_json) {
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
        ui_combo_select_text(s->voice_combo, wide);
        free(wide);
    }
    if (format) {
        wchar_t *wide = utf8_to_utf16(format);
        ui_combo_select_text(s->format_combo, wide);
        free(wide);
    }
    if (audio) {
        SendMessageW(s->optimize_check, BM_SETCHECK,
                     json_get_bool_value(audio, "optimize_text_preview", TRUE) ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    ui_refresh_multiline_scrollbars(s);
    cJSON_Delete(root);
}

void ui_update_multiline_scrollbar(HWND edit) {
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

void ui_refresh_multiline_scrollbars(MainState *s) {
    ui_update_multiline_scrollbar(s->style_edit);
    ui_update_multiline_scrollbar(s->text_edit);
    ui_update_multiline_scrollbar(s->preview_edit);
}
