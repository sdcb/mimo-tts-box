#pragma once

#include "app_types.h"

typedef struct HttpResponse {
    DWORD status_code;
    DWORD elapsed_ms;
    char *body;
    wchar_t *error_text;
} HttpResponse;

BOOL http_post_json(const wchar_t *url, const wchar_t *api_key, const char *json_body, HttpResponse *response);
void http_response_free(HttpResponse *response);
