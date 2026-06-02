#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "cJSON.h"
#include "win32_helpers.h"

static char *build_response_json(const char *base64) {
    cJSON *root = cJSON_CreateObject();
    cJSON *choices = cJSON_CreateArray();
    cJSON *choice = cJSON_CreateObject();
    cJSON *message = cJSON_CreateObject();
    cJSON *audio = cJSON_CreateObject();
    if (!root || !choices || !choice || !message || !audio) {
        cJSON_Delete(root);
        cJSON_Delete(choices);
        cJSON_Delete(choice);
        cJSON_Delete(message);
        cJSON_Delete(audio);
        return NULL;
    }
    cJSON_AddItemToObject(root, "choices", choices);
    cJSON_AddItemToArray(choices, choice);
    cJSON_AddItemToObject(choice, "message", message);
    cJSON_AddItemToObject(message, "audio", audio);
    cJSON_AddStringToObject(audio, "data", base64);
    cJSON_AddStringToObject(message, "final_text_preview", "selftest preview");
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static int run_case(const wchar_t *base64_path, const wchar_t *format, const wchar_t *out_dir) {
    char *base64 = NULL;
    BYTE *audio = NULL;
    DWORD audio_size = 0;
    wchar_t *preview = NULL;
    wchar_t *error = NULL;
    int rc = 1;
    if (!read_file_utf8(base64_path, &base64, NULL)) {
        wprintf(L"read failed: %ls\n", base64_path);
        goto done;
    }
    char *json = build_response_json(base64);
    if (!json) {
        printf("response json build failed\n");
        goto done;
    }
    if (!audio_parse_response(json, &audio, &audio_size, &preview, &error)) {
        wprintf(L"parse failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    AudioBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.data = audio;
    buffer.size = audio_size;
    wcsncpy(buffer.format, format, ARRAYSIZE(buffer.format) - 1);
    wcscpy(buffer.timestamp, L"selftest");

    wchar_t pcm_path[MAX_PATH];
    wchar_t wav_path[MAX_PATH];
    wchar_t mp3_path[MAX_PATH];
    swprintf(pcm_path, ARRAYSIZE(pcm_path), L"%ls\\%ls.pcm", out_dir, format);
    swprintf(wav_path, ARRAYSIZE(wav_path), L"%ls\\%ls.wav", out_dir, format);
    swprintf(mp3_path, ARRAYSIZE(mp3_path), L"%ls\\%ls.mp3", out_dir, format);

    if (!audio_save_to_file(&buffer, pcm_path, &error)) {
        wprintf(L"pcm save failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    free(error);
    error = NULL;
    if (!audio_save_to_file(&buffer, wav_path, &error)) {
        wprintf(L"wav save failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    free(error);
    error = NULL;
    if (!audio_save_to_file(&buffer, mp3_path, &error)) {
        wprintf(L"mp3 save failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    wprintf(L"%ls ok: %lu bytes, preview=%ls\n", format, (unsigned long)audio_size, preview ? preview : L"");
    rc = 0;
    free(json);

done:
    free(base64);
    free(audio);
    free(preview);
    free(error);
    return rc;
}

static int run_playback_smoke(const wchar_t *base64_path) {
    char *base64 = NULL;
    BYTE *audio = NULL;
    DWORD audio_size = 0;
    wchar_t *preview = NULL;
    wchar_t *error = NULL;
    int rc = 1;
    if (!read_file_utf8(base64_path, &base64, NULL)) {
        wprintf(L"playback read failed: %ls\n", base64_path);
        goto done;
    }
    char *json = build_response_json(base64);
    if (!json) {
        printf("playback response json build failed\n");
        goto done;
    }
    if (!audio_parse_response(json, &audio, &audio_size, &preview, &error)) {
        wprintf(L"playback parse failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    AudioBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.data = audio;
    buffer.size = audio_size;
    wcscpy(buffer.format, L"pcm");
    wcscpy(buffer.timestamp, L"selftest");
    if (!audio_system_init(NULL, &error)) {
        wprintf(L"audio init failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    if (!audio_play(&buffer, &error)) {
        wprintf(L"audio play failed: %ls\n", error ? error : L"(unknown)");
        audio_system_shutdown();
        free(json);
        goto done;
    }
    Sleep(150);
    audio_stop();
    audio_system_shutdown();
    wprintf(L"playback smoke ok: %lu bytes\n", (unsigned long)audio_size);
    rc = 0;
    free(json);

done:
    free(base64);
    free(audio);
    free(preview);
    free(error);
    return rc;
}

int wmain(int argc, wchar_t **argv) {
    if (argc != 5) {
        wprintf(L"usage: MimoTTSBoxSelfTest <pcm.base64> <wav.base64> <mp3.base64> <out-dir>\n");
        return 2;
    }
    ensure_directory(argv[4]);
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) {
        wprintf(L"MFStartup failed: 0x%08lx\n", (unsigned long)hr);
        return 1;
    }
    int rc = 0;
    rc |= run_case(argv[1], L"pcm", argv[4]);
    rc |= run_case(argv[2], L"wav", argv[4]);
    rc |= run_case(argv[3], L"mp3", argv[4]);
    rc |= run_playback_smoke(argv[1]);
    MFShutdown();
    return rc;
}
