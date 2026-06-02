#include "history_store.h"

#include "json_helpers.h"
#include "win32_helpers.h"

#include <stdlib.h>
#include <string.h>

static void assign_wstring_from_json(wchar_t **target, const cJSON *root, const char *name) {
    const char *value = json_get_string_value(root, name);
    if (value) {
        wchar_t *wide = utf8_to_utf16(value);
        if (wide) {
            free(*target);
            *target = wide;
        }
    }
}

static BOOL parse_metadata_text(const wchar_t *dir_name, const char *text, HistoryRecord *record) {
    memset(record, 0, sizeof(*record));
    record->dir_name = wcs_dup_or_empty(dir_name);
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        record->request_time = wcs_dup_or_empty(dir_name);
        record->elapsed_text = wcs_dup_or_empty(L"--");
        record->status_text = wcs_dup_or_empty(L"--");
        record->request_text = wcs_dup_or_empty(L"(metadata parse failed)");
        return FALSE;
    }
    assign_wstring_from_json(&record->request_time, root, "RequestTime");
    assign_wstring_from_json(&record->response_time, root, "ResponseTime");
    assign_wstring_from_json(&record->request_text, root, "RequestText");
    assign_wstring_from_json(&record->url, root, "Url");
    assign_wstring_from_json(&record->output_format, root, "OutputFormat");
    int status = json_get_int_value(root, "StatusCode", 0);
    int elapsed = json_get_int_value(root, "ElapsedMs", -1);
    record->audio_size = (DWORD)json_get_int_value(root, "AudioBytes", 0);
    wchar_t status_text[32];
    if (status > 0) {
        swprintf(status_text, 32, L"%d", status);
    } else {
        wcscpy(status_text, L"--");
    }
    wchar_t elapsed_text[32];
    if (elapsed >= 0) {
        swprintf(elapsed_text, 32, L"%dms", elapsed);
    } else {
        wcscpy(elapsed_text, L"--");
    }
    record->status_text = wcs_dup_or_empty(status_text);
    record->elapsed_text = wcs_dup_or_empty(elapsed_text);
    if (!record->request_time) {
        record->request_time = wcs_dup_or_empty(dir_name);
    }
    if (!record->request_text) {
        record->request_text = wcs_dup_or_empty(L"");
    }
    if (!record->output_format) {
        record->output_format = wcs_dup_or_empty(L"wav");
    }
    cJSON_Delete(root);
    return TRUE;
}

BOOL history_save_pending(const RequestTask *task) {
    if (!ensure_directory(APPDATA_DIR)) {
        return FALSE;
    }
    wchar_t *dir = path_join2(APPDATA_DIR, task->history_dir);
    if (!dir || !ensure_directory(dir)) {
        free(dir);
        return FALSE;
    }
    wchar_t *request_path = path_join2(dir, L"request.json");
    wchar_t *response_path = path_join2(dir, L"response.json");
    wchar_t *metadata_path = path_join2(dir, L"metadata.json");
    BOOL ok = request_path && response_path && metadata_path;
    if (ok) {
        ok = write_file_utf8(request_path, task->request_json ? task->request_json : "");
    }
    if (ok) {
        ok = write_file_utf8(response_path, "");
    }
    if (ok) {
        cJSON *root = cJSON_CreateObject();
        if (!root) {
            ok = FALSE;
        } else {
            json_add_wstring(root, "Url", task->url);
            json_add_wstring(root, "RequestTime", task->request_time);
            json_add_wstring(root, "ResponseTime", L"");
            cJSON_AddNumberToObject(root, "StatusCode", 0);
            cJSON_AddNumberToObject(root, "ElapsedMs", -1);
            json_add_wstring(root, "RequestText", task->audio_text);
            json_add_wstring(root, "OutputFormat", task->output_format);
            cJSON_AddNumberToObject(root, "AudioBytes", 0);
            cJSON_AddBoolToObject(root, "JsonOk", FALSE);
            cJSON_AddBoolToObject(root, "Pending", TRUE);
            char *json = cJSON_Print(root);
            ok = json && write_file_utf8(metadata_path, json);
            free(json);
            cJSON_Delete(root);
        }
    }
    free(request_path);
    free(response_path);
    free(metadata_path);
    free(dir);
    return ok;
}

BOOL history_save_result(const RequestResult *result) {
    if (!ensure_directory(APPDATA_DIR)) {
        return FALSE;
    }
    wchar_t *dir = path_join2(APPDATA_DIR, result->history_dir);
    if (!dir || !ensure_directory(dir)) {
        free(dir);
        return FALSE;
    }
    wchar_t *request_path = path_join2(dir, L"request.json");
    wchar_t *response_path = path_join2(dir, L"response.json");
    wchar_t *metadata_path = path_join2(dir, L"metadata.json");
    BOOL ok = request_path && response_path && metadata_path;
    if (ok) {
        ok = write_file_utf8(request_path, result->request_json ? result->request_json : "");
    }
    if (ok) {
        ok = write_file_utf8(response_path, result->response_text ? result->response_text : "");
    }
    if (ok) {
        cJSON *root = cJSON_CreateObject();
        if (!root) {
            ok = FALSE;
        } else {
            json_add_wstring(root, "Url", result->url);
            json_add_wstring(root, "RequestTime", result->request_time);
            json_add_wstring(root, "ResponseTime", result->response_time);
            cJSON_AddNumberToObject(root, "StatusCode", result->status_code);
            cJSON_AddNumberToObject(root, "ElapsedMs", result->elapsed_ms);
            json_add_wstring(root, "RequestText", result->audio_text);
            json_add_wstring(root, "OutputFormat", result->output_format);
            cJSON_AddNumberToObject(root, "AudioBytes", result->audio_size);
            cJSON_AddBoolToObject(root, "JsonOk", result->json_ok);
            cJSON_AddBoolToObject(root, "Pending", FALSE);
            char *json = cJSON_Print(root);
            ok = json && write_file_utf8(metadata_path, json);
            free(json);
            cJSON_Delete(root);
        }
    }
    free(request_path);
    free(response_path);
    free(metadata_path);
    free(dir);
    return ok;
}

static int compare_find_data_desc(const void *a, const void *b) {
    const WIN32_FIND_DATAW *fa = (const WIN32_FIND_DATAW *)a;
    const WIN32_FIND_DATAW *fb = (const WIN32_FIND_DATAW *)b;
    return wcscmp(fb->cFileName, fa->cFileName);
}

BOOL history_load_recent(HistoryRecord **records, int *count) {
    *records = NULL;
    *count = 0;
    DWORD attrs = GetFileAttributesW(APPDATA_DIR);
    if (attrs == INVALID_FILE_ATTRIBUTES) {
        return TRUE;
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return FALSE;
    }
    WIN32_FIND_DATAW *dirs = NULL;
    int dir_count = 0;
    int cap = 0;
    wchar_t *pattern = path_join2(APPDATA_DIR, L"*");
    WIN32_FIND_DATAW find_data;
    HANDLE find = FindFirstFileW(pattern, &find_data);
    free(pattern);
    if (find == INVALID_HANDLE_VALUE) {
        return TRUE;
    }
    do {
        if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
            wcscmp(find_data.cFileName, L".") != 0 &&
            wcscmp(find_data.cFileName, L"..") != 0) {
            if (dir_count == cap) {
                cap = cap ? cap * 2 : 64;
                WIN32_FIND_DATAW *new_dirs = (WIN32_FIND_DATAW *)realloc(dirs, (size_t)cap * sizeof(WIN32_FIND_DATAW));
                if (!new_dirs) {
                    break;
                }
                dirs = new_dirs;
            }
            dirs[dir_count++] = find_data;
        }
    } while (FindNextFileW(find, &find_data));
    FindClose(find);
    qsort(dirs, (size_t)dir_count, sizeof(WIN32_FIND_DATAW), compare_find_data_desc);
    int take = dir_count < 100 ? dir_count : 100;
    if (take == 0) {
        free(dirs);
        return TRUE;
    }
    HistoryRecord *loaded = (HistoryRecord *)calloc((size_t)take, sizeof(HistoryRecord));
    if (!loaded) {
        free(dirs);
        return FALSE;
    }
    int loaded_count = 0;
    for (int i = 0; i < take; ++i) {
        wchar_t *metadata_path = path_join3(APPDATA_DIR, dirs[i].cFileName, L"metadata.json");
        char *text = NULL;
        if (metadata_path && read_file_utf8(metadata_path, &text, NULL)) {
            parse_metadata_text(dirs[i].cFileName, text, &loaded[loaded_count++]);
        }
        free(text);
        free(metadata_path);
    }
    free(dirs);
    *records = loaded;
    *count = loaded_count;
    return TRUE;
}

BOOL history_load_detail(const wchar_t *dir_name, char **request_json, char **response_json, HistoryRecord *metadata) {
    *request_json = NULL;
    *response_json = NULL;
    memset(metadata, 0, sizeof(*metadata));
    wchar_t *request_path = path_join3(APPDATA_DIR, dir_name, L"request.json");
    wchar_t *response_path = path_join3(APPDATA_DIR, dir_name, L"response.json");
    wchar_t *metadata_path = path_join3(APPDATA_DIR, dir_name, L"metadata.json");
    char *metadata_text = NULL;
    BOOL ok = request_path && response_path && metadata_path &&
              read_file_utf8(request_path, request_json, NULL) &&
              read_file_utf8(response_path, response_json, NULL) &&
              read_file_utf8(metadata_path, &metadata_text, NULL);
    if (ok) {
        parse_metadata_text(dir_name, metadata_text, metadata);
    }
    free(metadata_text);
    free(request_path);
    free(response_path);
    free(metadata_path);
    return ok;
}

void history_free_records(HistoryRecord *records, int count) {
    if (!records) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        free_history_record(&records[i]);
    }
    free(records);
}
