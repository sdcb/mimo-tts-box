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
    cJSON *usage = cJSON_CreateObject();
    if (usage) {
        cJSON_AddItemToObject(root, "usage", usage);
        cJSON_AddNumberToObject(usage, "prompt_tokens", 259);
        cJSON_AddNumberToObject(usage, "completion_tokens", 104);
        cJSON_AddNumberToObject(usage, "total_tokens", 363);
    }
    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json;
}

static int save_one(AudioBuffer *buffer, const wchar_t *path) {
    wchar_t *error = NULL;
    if (!audio_save_to_file(buffer, path, &error)) {
        wprintf(L"save failed: %ls: %ls\n", path, error ? error : L"(unknown)");
        free(error);
        return 1;
    }
    free(error);
    return 0;
}

static void print_probe(const wchar_t *path) {
    wchar_t command[1024];
    swprintf(command, ARRAYSIZE(command),
             L"ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate,bit_rate -of default=noprint_wrappers=1:nokey=1 \"%ls\"",
             path);
    FILE *pipe = _wpopen(command, L"r");
    if (!pipe) {
        return;
    }
    wchar_t sample_rate[64] = L"";
    wchar_t bit_rate[64] = L"";
    fgetws(sample_rate, ARRAYSIZE(sample_rate), pipe);
    fgetws(bit_rate, ARRAYSIZE(bit_rate), pipe);
    _pclose(pipe);
    sample_rate[wcscspn(sample_rate, L"\r\n")] = L'\0';
    bit_rate[wcscspn(bit_rate, L"\r\n")] = L'\0';
    if (sample_rate[0]) {
        wprintf(L"probe %ls: sample_rate=%ls bit_rate=%ls\n", path, sample_rate, bit_rate[0] ? bit_rate : L"--");
    }
}

static int run_case(const wchar_t *base64_path, const wchar_t *format, const wchar_t *out_dir) {
    char *base64 = NULL;
    ParsedAudioResponse parsed = { 0 };
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
    if (!audio_parse_response(json, format, &parsed, &error)) {
        wprintf(L"parse failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    AudioBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.pcm_data = parsed.pcm_data;
    buffer.pcm_size = parsed.pcm_size;
    wcscpy(buffer.timestamp, L"selftest");

    wchar_t wav_path[MAX_PATH];
    wchar_t mp3_path[MAX_PATH];
    wchar_t aac_path[MAX_PATH];
    swprintf(wav_path, ARRAYSIZE(wav_path), L"%ls\\%ls.wav", out_dir, format);
    swprintf(mp3_path, ARRAYSIZE(mp3_path), L"%ls\\%ls.mp3", out_dir, format);
    swprintf(aac_path, ARRAYSIZE(aac_path), L"%ls\\%ls.aac", out_dir, format);

    if (save_one(&buffer, wav_path) || save_one(&buffer, mp3_path) || save_one(&buffer, aac_path)) {
        free(json);
        goto done;
    }
    print_probe(mp3_path);
    print_probe(aac_path);
    wprintf(L"%ls ok: %lu pcm bytes, duration=%dms, preview=%ls, tokens=%d/%d/%d\n",
            format, (unsigned long)parsed.pcm_size, parsed.audio_duration_ms,
            parsed.final_text_preview ? parsed.final_text_preview : L"",
            parsed.prompt_tokens, parsed.completion_tokens, parsed.total_tokens);
    rc = 0;
    free(json);

done:
    free(base64);
    audio_free_parsed_response(&parsed);
    free(error);
    return rc;
}

static int run_playback_smoke(const wchar_t *base64_path) {
    char *base64 = NULL;
    ParsedAudioResponse parsed = { 0 };
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
    if (!audio_parse_response(json, L"pcm", &parsed, &error)) {
        wprintf(L"playback parse failed: %ls\n", error ? error : L"(unknown)");
        free(json);
        goto done;
    }
    AudioBuffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.pcm_data = parsed.pcm_data;
    buffer.pcm_size = parsed.pcm_size;
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
    wprintf(L"playback smoke ok: %lu pcm bytes\n", (unsigned long)parsed.pcm_size);
    rc = 0;
    free(json);

done:
    free(base64);
    audio_free_parsed_response(&parsed);
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
