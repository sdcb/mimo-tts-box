#define WIN32_LEAN_AND_MEAN
#include "ui_dialogs.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "resource.h"
#include "ui_controls.h"
#include "ui_metrics.h"
#include "win32_helpers.h"

static const wchar_t *ABOUT_GITHUB_URL = L"https://github.com/sdcb/mimo-tts-box";
static const wchar_t *ABOUT_CJSON_URL = L"https://github.com/DaveGamble/cJSON";

static LRESULT CALLBACK config_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static INT_PTR CALLBACK about_dlg_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

static void open_url(HWND owner, const wchar_t *url) {
    HINSTANCE result = ShellExecuteW(owner, L"open", url, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        MessageBoxW(owner, L"无法打开链接。", APP_TITLE, MB_ICONWARNING);
    }
}

BOOL ui_show_config_dialog(HWND owner, AppConfig *config) {
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
    state.dpi = ui_dpi_from_hwnd(owner);
    RECT owner_rect;
    GetWindowRect(owner, &owner_rect);
    int width = ui_scale_for_dpi(520, state.dpi);
    int height = ui_scale_for_dpi(190, state.dpi);
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

void ui_show_about_dialog(HWND owner) {
    HINSTANCE instance = (HINSTANCE)GetWindowLongPtrW(owner, GWLP_HINSTANCE);
    DialogBoxParamW(instance, MAKEINTRESOURCEW(IDD_ABOUT), owner, about_dlg_proc, 0);
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
        s->ui_font = ui_create_font(hwnd);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)s);
        ui_make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"STATIC", L"Url", 0,
                             14, 20, 70, 24, 0);
        s->url_edit = ui_make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, WS_EX_CLIENTEDGE,
                                           L"EDIT", s->config->url, ES_AUTOHSCROLL,
                                           90, 18, 400, 24, 0);
        ui_make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"STATIC", L"API Key", 0,
                             14, 58, 70, 24, 0);
        s->api_edit = ui_make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, WS_EX_CLIENTEDGE,
                                           L"EDIT", s->config->api_key, ES_PASSWORD | ES_AUTOHSCROLL,
                                           90, 56, 400, 24, 0);
        ui_make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"BUTTON", L"确定", BS_DEFPUSHBUTTON,
                             318, 112, 80, 28, IDOK);
        ui_make_dialog_child(hwnd, cs->hInstance, s->ui_font, s->dpi, 0, L"BUTTON", L"取消", BS_PUSHBUTTON,
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
        return ui_paint_label_on_window_background(wparam);
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
