#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

#include "audio.h"
#include "config.h"
#include "ui_main.h"
#include "win32_helpers.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR command_line, int show_command) {
    (void)previous;
    (void)command_line;

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_WIN95_CLASSES | ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES | ICC_LINK_CLASS;
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    AppConfig config;
    wchar_t *error = NULL;
    if (!config_load(&config, &error) && error) {
        MessageBoxW(NULL, error, APP_TITLE, MB_ICONWARNING);
        free(error);
    }

    if (!ui_register_main_window(instance)) {
        free_config(&config);
        CoUninitialize();
        return 1;
    }
    HWND hwnd = ui_create_main_window(instance, &config);
    if (!hwnd) {
        free_config(&config);
        CoUninitialize();
        return 1;
    }

    ShowWindow(hwnd, show_command);
    UpdateWindow(hwnd);

    ACCEL accel_data[] = {
        { FVIRTKEY | FALT, 'S', IDC_SEND_BUTTON }
    };
    HACCEL accelerators = CreateAcceleratorTableW(accel_data, (int)ARRAYSIZE(accel_data));

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!accelerators || !TranslateAcceleratorW(hwnd, accelerators, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (accelerators) {
        DestroyAcceleratorTable(accelerators);
    }
    audio_system_shutdown();
    free_config(&config);
    CoUninitialize();
    return (int)msg.wParam;
}
