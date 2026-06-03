#pragma once

#include "ui_internal.h"

int ui_dpi_from_hwnd(HWND hwnd);
int ui_scale_for_dpi(int value, int dpi);
DpiMetrics ui_make_dpi_metrics(int dpi);
HFONT ui_create_font(HWND hwnd);
