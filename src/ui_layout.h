#pragma once

#include "ui_internal.h"

void ui_layout_controls(MainState *s);
DragMode ui_hit_test_splitter(MainState *s, int x, int y);
void ui_paint_splitters(MainState *s, HDC hdc);
