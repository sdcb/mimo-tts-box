#include "http_client.h"

#include "win32_helpers.h"

#include <winhttp.h>
#include <stdlib.h>
#include <string.h>

void http_response_free(HttpResponse *response) {
    if (!response) {
        return;
    }
    free(response->body);
    free(response->error_text);
    memset(response, 0, sizeof(*response));
}

static BOOL append_bytes(char **buffer, DWORD *used, DWORD *capacity, const BYTE *data, DWORD size) {
    if (*used + size + 1 > *capacity) {
        DWORD new_capacity = *capacity ? *capacity : 8192;
        while (new_capacity < *used + size + 1) {
            new_capacity *= 2;
        }
        char *new_buffer = (char *)realloc(*buffer, new_capacity);
        if (!new_buffer) {
            return FALSE;
        }
        *buffer = new_buffer;
        *capacity = new_capacity;
    }
    memcpy(*buffer + *used, data, size);
    *used += size;
    (*buffer)[*used] = '\0';
    return TRUE;
}

BOOL http_post_json(const wchar_t *url, const wchar_t *api_key, const char *json_body, HttpResponse *response) {
    memset(response, 0, sizeof(*response));
    DWORD start = GetTickCount();
    BOOL ok = FALSE;
    HINTERNET session = NULL;
    HINTERNET connect = NULL;
    HINTERNET request = NULL;
    URL_COMPONENTSW parts;
    wchar_t host[256];
    wchar_t path[2048];
    memset(&parts, 0, sizeof(parts));
    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));
    parts.dwStructSize = sizeof(parts);
    parts.lpszHostName = host;
    parts.dwHostNameLength = ARRAYSIZE(host);
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = ARRAYSIZE(path);
    parts.dwSchemeLength = (DWORD)-1;
    parts.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url, 0, 0, &parts)) {
        response->error_text = format_win32_error(GetLastError());
        goto done;
    }

    wchar_t full_path[2300];
    wcscpy(full_path, path[0] ? path : L"/");
    if (parts.lpszExtraInfo && parts.dwExtraInfoLength > 0) {
        wcsncat(full_path, parts.lpszExtraInfo, parts.dwExtraInfoLength);
    }

    session = WinHttpOpen(L"MimoTTSBox/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                          WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        response->error_text = format_win32_error(GetLastError());
        goto done;
    }
    connect = WinHttpConnect(session, host, parts.nPort, 0);
    if (!connect) {
        response->error_text = format_win32_error(GetLastError());
        goto done;
    }
    DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    request = WinHttpOpenRequest(connect, L"POST", full_path, NULL, WINHTTP_NO_REFERER,
                                 WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        response->error_text = format_win32_error(GetLastError());
        goto done;
    }
    WinHttpSetTimeouts(request, 30000, 30000, 30000, 120000);

    wchar_t *auth = NULL;
    size_t auth_len = wcslen(api_key ? api_key : L"") + 128;
    auth = (wchar_t *)calloc(auth_len, sizeof(wchar_t));
    if (!auth) {
        goto done;
    }
    swprintf(auth, auth_len, L"Content-Type: application/json\r\nAuthorization: Bearer %ls\r\n", api_key ? api_key : L"");

    DWORD body_len = (DWORD)strlen(json_body ? json_body : "");
    if (!WinHttpSendRequest(request, auth, (DWORD)-1, (LPVOID)json_body, body_len, body_len, 0)) {
        response->error_text = format_win32_error(GetLastError());
        free(auth);
        goto done;
    }
    free(auth);
    if (!WinHttpReceiveResponse(request, NULL)) {
        response->error_text = format_win32_error(GetLastError());
        goto done;
    }
    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
    response->status_code = status;

    char *buffer = NULL;
    DWORD used = 0;
    DWORD capacity = 0;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available)) {
            response->error_text = format_win32_error(GetLastError());
            break;
        }
        if (available == 0) {
            ok = TRUE;
            break;
        }
        BYTE *chunk = (BYTE *)malloc(available);
        if (!chunk) {
            break;
        }
        DWORD read = 0;
        BOOL read_ok = WinHttpReadData(request, chunk, available, &read);
        if (read_ok && read > 0) {
            read_ok = append_bytes(&buffer, &used, &capacity, chunk, read);
        }
        free(chunk);
        if (!read_ok) {
            response->error_text = format_win32_error(GetLastError());
            break;
        }
    }
    response->body = buffer ? buffer : str_dup_or_empty("");

done:
    response->elapsed_ms = GetTickCount() - start;
    if (request) {
        WinHttpCloseHandle(request);
    }
    if (connect) {
        WinHttpCloseHandle(connect);
    }
    if (session) {
        WinHttpCloseHandle(session);
    }
    return ok;
}
