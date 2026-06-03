#define WIN32_LEAN_AND_MEAN
#include "ui_metrics.h"

#include <string.h>

int ui_dpi_from_hwnd(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSX) : 96;
    if (hdc) {
        ReleaseDC(hwnd, hdc);
    }
    return dpi > 0 ? dpi : 96;
}

int ui_scale_for_dpi(int value, int dpi) {
    return MulDiv(value, dpi > 0 ? dpi : 96, 96);
}

DpiMetrics ui_make_dpi_metrics(int dpi) {
    DpiMetrics metrics;
    memset(&metrics, 0, sizeof(metrics));
    metrics.margin = ui_scale_for_dpi(8, dpi);
    metrics.url_h = ui_scale_for_dpi(30, dpi);
    metrics.min_w = ui_scale_for_dpi(640, dpi);
    metrics.min_h = ui_scale_for_dpi(480, dpi);
    metrics.window_w = ui_scale_for_dpi(800, dpi);
    metrics.window_h = ui_scale_for_dpi(600, dpi);
    metrics.send_w = ui_scale_for_dpi(120, dpi);
    metrics.splitter = ui_scale_for_dpi(SPLITTER_HIT_SIZE, dpi);
    metrics.splitter_line = max(1, ui_scale_for_dpi(SPLITTER_LINE_SIZE, dpi));
    metrics.initial_main_split = ui_scale_for_dpi(260, dpi);
    metrics.initial_vertical_split = ui_scale_for_dpi(270, dpi);
    metrics.initial_request_split = ui_scale_for_dpi(260, dpi);
    metrics.main_split_min = ui_scale_for_dpi(180, dpi);
    metrics.main_right_min = ui_scale_for_dpi(300, dpi);
    metrics.split_pane_min = ui_scale_for_dpi(170, dpi);
    metrics.label_h = ui_scale_for_dpi(20, dpi);
    metrics.bottom_controls_h = ui_scale_for_dpi(34, dpi);
    metrics.controls_y_offset = ui_scale_for_dpi(5, dpi);
    metrics.combo_drop_h = ui_scale_for_dpi(400, dpi);
    metrics.voice_w = ui_scale_for_dpi(120, dpi);
    metrics.format_x = ui_scale_for_dpi(130, dpi);
    metrics.format_w = ui_scale_for_dpi(90, dpi);
    metrics.optimize_x = ui_scale_for_dpi(230, dpi);
    metrics.optimize_w = ui_scale_for_dpi(170, dpi);
    metrics.auto_play_x = ui_scale_for_dpi(405, dpi);
    metrics.auto_play_w = ui_scale_for_dpi(115, dpi);
    metrics.check_h = ui_scale_for_dpi(24, dpi);
    metrics.resp_button_w = ui_scale_for_dpi(88, dpi);
    metrics.resp_button_h = ui_scale_for_dpi(28, dpi);
    metrics.resp_button_step = ui_scale_for_dpi(96, dpi);
    metrics.resp_gap = ui_scale_for_dpi(6, dpi);
    metrics.status_h = ui_scale_for_dpi(24, dpi);
    metrics.status_code_w = ui_scale_for_dpi(96, dpi);
    metrics.status_elapsed_w = ui_scale_for_dpi(128, dpi);
    metrics.status_duration_w = ui_scale_for_dpi(128, dpi);
    metrics.status_tokens_min_w = ui_scale_for_dpi(180, dpi);
    return metrics;
}

HFONT ui_create_font(HWND hwnd) {
    HDC hdc = GetDC(hwnd);
    int ui_height = -MulDiv(9, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(hwnd, hdc);
    return CreateFontW(ui_height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}
