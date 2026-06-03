#define WIN32_LEAN_AND_MEAN
#include "ui_layout.h"

#include "ui_controls.h"
#include "ui_metrics.h"
#include "ui_response_view.h"

void ui_layout_controls(MainState *s) {
    DpiMetrics metrics = ui_make_dpi_metrics(s->dpi);
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
    ui_layout_response_status_bar(s, &metrics, right_x, resp_y + resp_h - metrics.status_h, right_w,
                                  metrics.status_h);
    ui_refresh_multiline_scrollbars(s);
    InvalidateRect(s->hwnd, NULL, TRUE);
}

static BOOL hit_rect_point(RECT rc, int x, int y) {
    return x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom;
}

DragMode ui_hit_test_splitter(MainState *s, int x, int y) {
    DpiMetrics metrics = ui_make_dpi_metrics(s->dpi);
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

void ui_paint_splitters(MainState *s, HDC hdc) {
    DpiMetrics metrics = ui_make_dpi_metrics(s->dpi);
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
