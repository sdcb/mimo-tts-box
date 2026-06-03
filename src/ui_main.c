#define WIN32_LEAN_AND_MEAN
#include "ui_main.h"

#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

#include "audio.h"
#include "config.h"
#include "resource.h"
#include "ui_controls.h"
#include "ui_dialogs.h"
#include "ui_history_list.h"
#include "ui_internal.h"
#include "ui_layout.h"
#include "ui_metrics.h"
#include "ui_request_flow.h"
#include "ui_response_view.h"
#include "win32_helpers.h"

static LRESULT CALLBACK main_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static MainState *state_from_hwnd(HWND hwnd) {
    return (MainState *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
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
    s->dpi = ui_dpi_from_hwnd(hwnd);
    DpiMetrics metrics = ui_make_dpi_metrics(s->dpi);
    s->main_split = metrics.initial_main_split;
    s->vertical_split = metrics.initial_vertical_split;
    s->request_split = metrics.initial_request_split;

    HDC hdc = GetDC(hwnd);
    int ui_height = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);
    s->ui_font = ui_create_font(hwnd);
    s->emoji_font = CreateFontW(ui_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Emoji");
    s->mono_font = (HFONT)GetStockObject(ANSI_FIXED_FONT);

    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
    create_main_menu(hwnd);
    s->url_edit = ui_make_child(s, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, WS_EX_CLIENTEDGE, IDC_URL_EDIT);
    s->send_button = ui_make_child(s, L"BUTTON", L"发送(&S)", BS_PUSHBUTTON, 0, IDC_SEND_BUTTON);
    s->list = ui_make_child(s, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL, WS_EX_CLIENTEDGE, IDC_HISTORY_LIST);
    ui_configure_list_columns(s->list, s->dpi);
    s->style_label = ui_make_child(s, L"STATIC", L"风格控制", 0, 0, 0);
    s->text_label = ui_make_child(s, L"STATIC", L"音频合成文本", 0, 0, 0);
    s->style_edit = ui_make_child(s, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, IDC_STYLE_EDIT);
    s->text_edit = ui_make_child(s, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, WS_EX_CLIENTEDGE, IDC_TEXT_EDIT);
    s->voice_combo = ui_make_child(s, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL, 0, IDC_VOICE_COMBO);
    s->format_combo = ui_make_child(s, L"COMBOBOX", L"", CBS_DROPDOWNLIST, 0, IDC_FORMAT_COMBO);
    s->optimize_check = ui_make_child(s, L"BUTTON", L"optimize_text_preview", BS_AUTOCHECKBOX, 0, IDC_OPTIMIZE_CHECK);
    s->auto_play_check = ui_make_child(s, L"BUTTON", L"下载后播放", BS_AUTOCHECKBOX, 0, IDC_AUTO_PLAY_CHECK);
    s->status_bar = ui_make_child(s, STATUSCLASSNAMEW, L"", CCS_NOPARENTALIGN | CCS_NORESIZE | SBARS_TOOLTIPS, 0, IDC_STATUS_BAR);
    s->preview_edit = ui_make_child(s, L"EDIT", L"", WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL, WS_EX_CLIENTEDGE, IDC_PREVIEW_EDIT);
    s->play_button = ui_make_child(s, L"BUTTON", L"\x25b6\xfe0f 播放", BS_PUSHBUTTON, 0, IDC_PLAY_BUTTON);
    s->stop_button = ui_make_child(s, L"BUTTON", L"\x23f9\xfe0f 停止", BS_PUSHBUTTON, 0, IDC_STOP_BUTTON);
    s->save_button = ui_make_child(s, L"BUTTON", L"\xd83d\xddc3\xfe0f 保存", BS_PUSHBUTTON, 0, IDC_SAVE_BUTTON);
    SendMessageW(s->play_button, WM_SETFONT, (WPARAM)s->emoji_font, TRUE);
    SendMessageW(s->stop_button, WM_SETFONT, (WPARAM)s->emoji_font, TRUE);
    SendMessageW(s->save_button, WM_SETFONT, (WPARAM)s->emoji_font, TRUE);

    ui_populate_request_controls(s);
    ui_load_history_into_list(s);
    wchar_t *audio_error = NULL;
    if (!audio_system_init(hwnd, &audio_error) && audio_error) {
        MessageBoxW(hwnd, audio_error, APP_TITLE, MB_ICONWARNING);
        free(audio_error);
    }
    ui_layout_controls(s);
    ui_set_response_status_bar_text(s, L"--", L"--", L"--", L"--", L"--", L"--");
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
    int dpi = ui_dpi_from_hwnd(NULL);
    DpiMetrics metrics = ui_make_dpi_metrics(dpi);
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
            ui_layout_controls(s);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO *info = (MINMAXINFO *)lparam;
        int dpi = s ? s->dpi : 96;
        DpiMetrics metrics = ui_make_dpi_metrics(dpi);
        info->ptMinTrackSize.x = metrics.min_w;
        info->ptMinTrackSize.y = metrics.min_h;
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        HWND child = (HWND)lparam;
        if (s && (child == s->style_label || child == s->text_label ||
                  child == s->optimize_check || child == s->auto_play_check)) {
            return ui_paint_label_on_window_background(wparam);
        }
        break;
    }
    case WM_SYSCHAR:
    case WM_SYSKEYDOWN:
        if (s && ui_is_alt_s_message(msg, wparam, lparam)) {
            ui_trigger_send_command(s);
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
            ui_update_multiline_scrollbar((HWND)lparam);
            return 0;
        }
        switch (LOWORD(wparam)) {
        case IDC_SEND_BUTTON:
            ui_on_send(s);
            return 0;
        case IDC_PLAY_BUTTON:
            ui_on_play(s);
            return 0;
        case IDC_STOP_BUTTON:
            audio_stop();
            EnableWindow(s->play_button, TRUE);
            EnableWindow(s->stop_button, FALSE);
            return 0;
        case IDC_SAVE_BUTTON:
            ui_on_save(s);
            return 0;
        case IDM_FILE_CONFIG:
            ui_update_config_from_controls(s);
            if (ui_show_config_dialog(hwnd, s->config)) {
                set_control_text(s->url_edit, s->config->url);
                config_save(s->config, NULL);
            }
            return 0;
        case IDM_FILE_EXIT:
            DestroyWindow(hwnd);
            return 0;
        case IDM_HELP_ABOUT:
            ui_show_about_dialog(hwnd);
            return 0;
        }
        break;
    case WM_NOTIFY:
        if (s && ((LPNMHDR)lparam)->idFrom == IDC_HISTORY_LIST && ((LPNMHDR)lparam)->code == NM_DBLCLK) {
            LPNMITEMACTIVATE act = (LPNMITEMACTIVATE)lparam;
            if (act->iItem >= 0) {
                ui_activate_history_record(s, act->iItem);
            }
            return 0;
        }
        break;
    case WM_TIMER:
        if (s && wparam == REQUEST_TIME_TIMER_ID) {
            ui_refresh_request_time_column(s->list);
            return 0;
        }
        break;
    case WM_LBUTTONDOWN:
        if (s) {
            int x = GET_X_LPARAM(lparam);
            int y = GET_Y_LPARAM(lparam);
            s->drag = ui_hit_test_splitter(s, x, y);
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
                DragMode hit = ui_hit_test_splitter(s, x, y);
                SetCursor(LoadCursor(NULL, hit == DRAG_VERTICAL ? IDC_SIZENS : (hit == DRAG_NONE ? IDC_ARROW : IDC_SIZEWE)));
            } else {
                DpiMetrics metrics = ui_make_dpi_metrics(s->dpi);
                int main_top = metrics.margin + metrics.url_h + metrics.margin;
                int right_x = metrics.margin + s->main_split + metrics.splitter;
                if (s->drag == DRAG_MAIN) {
                    s->main_split = x - metrics.margin;
                } else if (s->drag == DRAG_VERTICAL) {
                    s->vertical_split = y - main_top;
                } else if (s->drag == DRAG_REQUEST) {
                    s->request_split = x - right_x;
                }
                ui_layout_controls(s);
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
            ui_paint_splitters(s, hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        break;
    case WM_APP_REQUEST_DONE:
        if (s) {
            ui_on_request_done(s, (RequestResult *)lparam);
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
            ui_clear_list_lparams(s->list);
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
