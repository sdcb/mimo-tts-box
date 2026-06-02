#pragma once

#include "app_types.h"
#include "cJSON.h"

const cJSON *json_get_object(const cJSON *parent, const char *name);
const char *json_get_string_value(const cJSON *parent, const char *name);
int json_get_int_value(const cJSON *parent, const char *name, int fallback);
BOOL json_get_bool_value(const cJSON *parent, const char *name, BOOL fallback);
cJSON *json_add_wstring(cJSON *object, const char *name, const wchar_t *value);
