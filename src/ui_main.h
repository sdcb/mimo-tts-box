#pragma once

#include "app_types.h"

BOOL ui_register_main_window(HINSTANCE instance);
HWND ui_create_main_window(HINSTANCE instance, AppConfig *config);
