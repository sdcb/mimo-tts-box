#include "win32_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void safe_free(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

wchar_t *wcs_dup_or_empty(const wchar_t *text) {
    const wchar_t *src = text ? text : L"";
    size_t len = wcslen(src);
    wchar_t *copy = (wchar_t *)calloc(len + 1, sizeof(wchar_t));
    if (copy) {
        memcpy(copy, src, (len + 1) * sizeof(wchar_t));
    }
    return copy;
}

char *str_dup_or_empty(const char *text) {
    const char *src = text ? text : "";
    size_t len = strlen(src);
    char *copy = (char *)calloc(len + 1, 1);
    if (copy) {
        memcpy(copy, src, len + 1);
    }
    return copy;
}

char *utf16_to_utf8(const wchar_t *text) {
    if (!text) {
        return str_dup_or_empty("");
    }
    int count = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (count <= 0) {
        return NULL;
    }
    char *buf = (char *)calloc((size_t)count, 1);
    if (!buf) {
        return NULL;
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, buf, count, NULL, NULL)) {
        free(buf);
        return NULL;
    }
    return buf;
}

wchar_t *utf8_to_utf16(const char *text) {
    if (!text) {
        return wcs_dup_or_empty(L"");
    }
    int count = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (count <= 0) {
        return NULL;
    }
    wchar_t *buf = (wchar_t *)calloc((size_t)count, sizeof(wchar_t));
    if (!buf) {
        return NULL;
    }
    if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, buf, count)) {
        free(buf);
        return NULL;
    }
    return buf;
}

wchar_t *format_win32_error(DWORD error_code) {
    wchar_t *msg = NULL;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error_code, 0, (LPWSTR)&msg, 0, NULL);
    if (!msg) {
        wchar_t fallback[64];
        swprintf(fallback, 64, L"Win32 error %lu", error_code);
        return wcs_dup_or_empty(fallback);
    }
    wchar_t *copy = wcs_dup_or_empty(msg);
    LocalFree(msg);
    return copy;
}

wchar_t *format_hresult_error(HRESULT hr) {
    wchar_t text[96];
    swprintf(text, 96, L"HRESULT 0x%08lX", (unsigned long)hr);
    return wcs_dup_or_empty(text);
}

wchar_t *path_join2(const wchar_t *a, const wchar_t *b) {
    size_t alen = wcslen(a);
    size_t blen = wcslen(b);
    BOOL slash = alen > 0 && (a[alen - 1] == L'\\' || a[alen - 1] == L'/');
    wchar_t *path = (wchar_t *)calloc(alen + blen + (slash ? 1 : 2), sizeof(wchar_t));
    if (!path) {
        return NULL;
    }
    wcscpy(path, a);
    if (!slash) {
        wcscat(path, L"\\");
    }
    wcscat(path, b);
    return path;
}

wchar_t *path_join3(const wchar_t *a, const wchar_t *b, const wchar_t *c) {
    wchar_t *ab = path_join2(a, b);
    if (!ab) {
        return NULL;
    }
    wchar_t *abc = path_join2(ab, c);
    free(ab);
    return abc;
}

BOOL ensure_directory(const wchar_t *path) {
    if (CreateDirectoryW(path, NULL)) {
        return TRUE;
    }
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

BOOL read_file_utf8(const wchar_t *path, char **out_text, DWORD *out_size) {
    *out_text = NULL;
    if (out_size) {
        *out_size = 0;
    }
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    LARGE_INTEGER size;
    if (!GetFileSizeEx(file, &size) || size.QuadPart > 0x7fffffff) {
        CloseHandle(file);
        return FALSE;
    }
    char *buf = (char *)calloc((size_t)size.QuadPart + 1, 1);
    if (!buf) {
        CloseHandle(file);
        return FALSE;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(file, buf, (DWORD)size.QuadPart, &read, NULL);
    CloseHandle(file);
    if (!ok) {
        free(buf);
        return FALSE;
    }
    buf[read] = '\0';
    *out_text = buf;
    if (out_size) {
        *out_size = read;
    }
    return TRUE;
}

BOOL write_file_utf8(const wchar_t *path, const char *text) {
    return write_file_bytes(path, (const BYTE *)(text ? text : ""), (DWORD)strlen(text ? text : ""));
}

BOOL write_file_bytes(const wchar_t *path, const BYTE *data, DWORD size) {
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(file, data, size, &written, NULL);
    CloseHandle(file);
    return ok && written == size;
}

BOOL file_exists(const wchar_t *path) {
    DWORD attrs = GetFileAttributesW(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

void current_timestamp(wchar_t out[32]) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(out, 32, L"%04u%02u%02u_%02u%02u%02u_%03u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

void current_display_time(wchar_t out[64]) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(out, 64, L"%04u-%02u-%02u %02u:%02u:%02u.%03u",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
}

wchar_t *format_elapsed(DWORD elapsed_ms) {
    wchar_t text[32];
    swprintf(text, 32, L"%lums", (unsigned long)elapsed_ms);
    return wcs_dup_or_empty(text);
}

void set_control_text(HWND hwnd, const wchar_t *text) {
    SetWindowTextW(hwnd, text ? text : L"");
}

wchar_t *get_control_text_alloc(HWND hwnd) {
    int len = GetWindowTextLengthW(hwnd);
    wchar_t *buf = (wchar_t *)calloc((size_t)len + 1, sizeof(wchar_t));
    if (!buf) {
        return NULL;
    }
    GetWindowTextW(hwnd, buf, len + 1);
    return buf;
}

void free_config(AppConfig *config) {
    if (!config) {
        return;
    }
    free(config->url);
    free(config->api_key);
    free(config->request.style_control);
    free(config->request.audio_text);
    free(config->request.voice);
    free(config->request.output_format);
    memset(config, 0, sizeof(*config));
}

void free_active_response(ActiveResponse *response) {
    if (!response) {
        return;
    }
    free(response->request_time);
    free(response->response_time);
    free(response->preview_text);
    free(response->raw_response);
    free(response->output_format);
    free(response->audio.data);
    memset(response, 0, sizeof(*response));
}

void free_history_record(HistoryRecord *record) {
    if (!record) {
        return;
    }
    free(record->dir_name);
    free(record->request_time);
    free(record->response_time);
    free(record->elapsed_text);
    free(record->status_text);
    free(record->request_text);
    free(record->url);
    free(record->output_format);
    memset(record, 0, sizeof(*record));
}

void free_request_task(RequestTask *task) {
    if (!task) {
        return;
    }
    free(task->history_dir);
    free(task->url);
    free(task->api_key);
    free(task->request_time);
    free(task->style_control);
    free(task->audio_text);
    free(task->voice);
    free(task->output_format);
    free(task->request_json);
    free(task);
}

void free_request_result(RequestResult *result) {
    if (!result) {
        return;
    }
    free(result->history_dir);
    free(result->request_time);
    free(result->response_time);
    free(result->url);
    free(result->audio_text);
    free(result->output_format);
    free(result->request_json);
    free(result->response_text);
    free(result->error_text);
    free(result->final_text_preview);
    free(result->audio_data);
    free(result);
}
