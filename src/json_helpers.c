#include "json_helpers.h"

#include "win32_helpers.h"

#include <stdlib.h>

const cJSON *json_get_object(const cJSON *parent, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsObject(item) ? item : NULL;
}

const char *json_get_string_value(const cJSON *parent, const char *name) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : NULL;
}

int json_get_int_value(const cJSON *parent, const char *name, int fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

BOOL json_get_bool_value(const cJSON *parent, const char *name, BOOL fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(parent, name);
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return fallback;
}

cJSON *json_add_wstring(cJSON *object, const char *name, const wchar_t *value) {
    char *utf8 = utf16_to_utf8(value ? value : L"");
    if (!utf8) {
        return NULL;
    }
    cJSON *item = cJSON_AddStringToObject(object, name, utf8);
    free(utf8);
    return item;
}
