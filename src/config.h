#pragma once

#include "app_types.h"

void config_set_defaults(AppConfig *config);
BOOL config_load(AppConfig *config, wchar_t **error_text);
BOOL config_save(const AppConfig *config, wchar_t **error_text);
BOOL config_has_api_key(const AppConfig *config);
