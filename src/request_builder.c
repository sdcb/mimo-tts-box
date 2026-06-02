#include "request_builder.h"

#include "json_helpers.h"
#include "win32_helpers.h"

#include <stdlib.h>

char *build_request_json(const RequestSettings *settings) {
    cJSON *root = cJSON_CreateObject();
    cJSON *messages = cJSON_CreateArray();
    cJSON *style_msg = cJSON_CreateObject();
    cJSON *text_msg = cJSON_CreateObject();
    cJSON *audio = cJSON_CreateObject();
    if (!root || !messages || !style_msg || !text_msg || !audio) {
        cJSON_Delete(root);
        cJSON_Delete(messages);
        cJSON_Delete(style_msg);
        cJSON_Delete(text_msg);
        cJSON_Delete(audio);
        return NULL;
    }

    cJSON_AddStringToObject(root, "model", "mimo-v2.5-tts");
    cJSON_AddItemToObject(root, "messages", messages);
    cJSON_AddStringToObject(style_msg, "role", "user");
    json_add_wstring(style_msg, "content", settings->style_control);
    cJSON_AddStringToObject(text_msg, "role", "assistant");
    json_add_wstring(text_msg, "content", settings->audio_text);
    cJSON_AddItemToArray(messages, style_msg);
    cJSON_AddItemToArray(messages, text_msg);

    cJSON_AddItemToObject(root, "audio", audio);
    json_add_wstring(audio, "format", settings->output_format);
    json_add_wstring(audio, "voice", settings->voice);
    cJSON_AddBoolToObject(audio, "optimize_text_preview", settings->optimize_text_preview);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}
