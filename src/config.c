#include "config.h"

#include "json_helpers.h"
#include "win32_helpers.h"

#include <dpapi.h>
#include <wincrypt.h>
#include <stdlib.h>
#include <string.h>

static const wchar_t *DEFAULT_STYLE =
    L"用慵懒又带点宠溺的语气，像午后刚睡醒的猫一样，慢悠悠地跟恋人撒娇。";
static const wchar_t *DEFAULT_TEXT =
    L"[四川话]（委屈）哎……你怎么才回来呀……我等你等到都困了……下次早点嘛，好不好？";

static wchar_t *settings_path(void) {
    return wcs_dup_or_empty(L"appsettings.json");
}

void config_set_defaults(AppConfig *config) {
    memset(config, 0, sizeof(*config));
    config->url = wcs_dup_or_empty(DEFAULT_URL);
    config->api_key = wcs_dup_or_empty(L"");
    config->request.style_control = wcs_dup_or_empty(DEFAULT_STYLE);
    config->request.audio_text = wcs_dup_or_empty(DEFAULT_TEXT);
    config->request.voice = wcs_dup_or_empty(L"mimo_default");
    config->request.output_format = wcs_dup_or_empty(L"wav");
    config->request.optimize_text_preview = TRUE;
    config->request.auto_play_on_download = FALSE;
}

BOOL config_has_api_key(const AppConfig *config) {
    return config && config->api_key && config->api_key[0] != L'\0';
}

static char *protect_api_key(const wchar_t *api_key) {
    if (!api_key || api_key[0] == L'\0') {
        return str_dup_or_empty("");
    }
    char *utf8 = utf16_to_utf8(api_key);
    if (!utf8) {
        return NULL;
    }
    DATA_BLOB input;
    DATA_BLOB output;
    input.pbData = (BYTE *)utf8;
    input.cbData = (DWORD)strlen(utf8);
    memset(&output, 0, sizeof(output));
    if (!CryptProtectData(&input, L"MimoTTSBox API Key", NULL, NULL, NULL, 0, &output)) {
        free(utf8);
        return NULL;
    }
    DWORD chars = 0;
    CryptBinaryToStringA(output.pbData, output.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &chars);
    char *base64 = (char *)calloc(chars + 1, 1);
    if (base64) {
        CryptBinaryToStringA(output.pbData, output.cbData, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, base64, &chars);
    }
    LocalFree(output.pbData);
    free(utf8);
    return base64;
}

static wchar_t *unprotect_api_key(const char *base64) {
    if (!base64 || base64[0] == '\0') {
        return wcs_dup_or_empty(L"");
    }
    DWORD bytes = 0;
    if (!CryptStringToBinaryA(base64, 0, CRYPT_STRING_BASE64, NULL, &bytes, NULL, NULL)) {
        return wcs_dup_or_empty(L"");
    }
    BYTE *cipher = (BYTE *)calloc(bytes, 1);
    if (!cipher) {
        return NULL;
    }
    if (!CryptStringToBinaryA(base64, 0, CRYPT_STRING_BASE64, cipher, &bytes, NULL, NULL)) {
        free(cipher);
        return wcs_dup_or_empty(L"");
    }
    DATA_BLOB input;
    DATA_BLOB output;
    input.pbData = cipher;
    input.cbData = bytes;
    memset(&output, 0, sizeof(output));
    if (!CryptUnprotectData(&input, NULL, NULL, NULL, NULL, 0, &output)) {
        free(cipher);
        return wcs_dup_or_empty(L"");
    }
    char *utf8 = (char *)calloc(output.cbData + 1, 1);
    wchar_t *plain = NULL;
    if (utf8) {
        memcpy(utf8, output.pbData, output.cbData);
        plain = utf8_to_utf16(utf8);
        free(utf8);
    }
    LocalFree(output.pbData);
    free(cipher);
    return plain ? plain : wcs_dup_or_empty(L"");
}

static void replace_wstring(wchar_t **target, const char *utf8) {
    if (!utf8) {
        return;
    }
    wchar_t *value = utf8_to_utf16(utf8);
    if (value) {
        free(*target);
        *target = value;
    }
}

BOOL config_load(AppConfig *config, wchar_t **error_text) {
    if (error_text) {
        *error_text = NULL;
    }
    config_set_defaults(config);
    wchar_t *path = settings_path();
    if (!path || !file_exists(path)) {
        free(path);
        return TRUE;
    }
    char *text = NULL;
    if (!read_file_utf8(path, &text, NULL)) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"无法读取 appsettings.json。");
        }
        free(path);
        return FALSE;
    }
    cJSON *root = cJSON_Parse(text);
    if (!root) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"appsettings.json 不是合法 JSON，已使用默认配置。");
        }
        free(text);
        free(path);
        return FALSE;
    }
    replace_wstring(&config->url, json_get_string_value(root, "Url"));
    const char *api_key = json_get_string_value(root, "ApiKey");
    wchar_t *decrypted = unprotect_api_key(api_key);
    if (decrypted) {
        free(config->api_key);
        config->api_key = decrypted;
    }
    const cJSON *request = json_get_object(root, "Request");
    if (request) {
        replace_wstring(&config->request.style_control, json_get_string_value(request, "StyleControl"));
        replace_wstring(&config->request.audio_text, json_get_string_value(request, "AudioSynthesisText"));
        replace_wstring(&config->request.voice, json_get_string_value(request, "Voice"));
        replace_wstring(&config->request.output_format, json_get_string_value(request, "OutputFormat"));
        config->request.optimize_text_preview = json_get_bool_value(request, "OptimizeTextPreview", TRUE);
        config->request.auto_play_on_download = json_get_bool_value(request, "AutoPlayOnDownload", FALSE);
    }
    cJSON_Delete(root);
    free(text);
    free(path);
    return TRUE;
}

BOOL config_save(const AppConfig *config, wchar_t **error_text) {
    if (error_text) {
        *error_text = NULL;
    }
    cJSON *root = cJSON_CreateObject();
    cJSON *request = cJSON_CreateObject();
    if (!root || !request) {
        cJSON_Delete(root);
        cJSON_Delete(request);
        return FALSE;
    }
    json_add_wstring(root, "Url", config->url);
    char *protected_key = protect_api_key(config->api_key);
    cJSON_AddStringToObject(root, "ApiKey", protected_key ? protected_key : "");
    cJSON_AddItemToObject(root, "Request", request);
    json_add_wstring(request, "StyleControl", config->request.style_control);
    json_add_wstring(request, "AudioSynthesisText", config->request.audio_text);
    json_add_wstring(request, "Voice", config->request.voice);
    json_add_wstring(request, "OutputFormat", config->request.output_format);
    cJSON_AddBoolToObject(request, "OptimizeTextPreview", config->request.optimize_text_preview);
    cJSON_AddBoolToObject(request, "AutoPlayOnDownload", config->request.auto_play_on_download);
    char *json = cJSON_Print(root);
    wchar_t *path = settings_path();
    BOOL ok = json && path && write_file_utf8(path, json);
    if (!ok && error_text) {
        *error_text = wcs_dup_or_empty(L"无法保存 appsettings.json。");
    }
    free(path);
    free(json);
    free(protected_key);
    cJSON_Delete(root);
    return ok;
}
