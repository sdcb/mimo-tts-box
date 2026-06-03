#pragma once

#include "ui_internal.h"

void ui_set_active_from_result(MainState *s, RequestResult *result);
void ui_set_response_status_bar_text(MainState *s, const wchar_t *status_code, const wchar_t *elapsed,
                                     const wchar_t *duration, const wchar_t *prompt_tokens,
                                     const wchar_t *completion_tokens, const wchar_t *total_tokens);
void ui_refresh_response_view(MainState *s, BOOL success_like);
void ui_layout_response_status_bar(MainState *s, const DpiMetrics *metrics, int x, int y, int width, int height);
void ui_on_play(MainState *s);
void ui_on_save(MainState *s);
