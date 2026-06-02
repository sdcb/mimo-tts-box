#define COBJMACROS
#include "audio.h"

#include "json_helpers.h"
#include "win32_helpers.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <propvarutil.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <xaudio2.h>
#include <stdlib.h>
#include <string.h>

static IXAudio2 *g_xaudio = NULL;
static IXAudio2MasteringVoice *g_master_voice = NULL;
static IXAudio2SourceVoice *g_source_voice = NULL;
static BYTE *g_playback_pcm = NULL;
static DWORD g_playback_pcm_size = 0;
static HANDLE g_monitor_thread = NULL;
static HWND g_notify_hwnd = NULL;
static volatile LONG g_playing = 0;
static volatile LONG g_playback_generation = 0;

typedef struct PlaybackMonitorArgs {
    LONG generation;
} PlaybackMonitorArgs;

static DWORD WINAPI playback_monitor_thread(void *arg) {
    PlaybackMonitorArgs *args = (PlaybackMonitorArgs *)arg;
    LONG generation = args ? args->generation : 0;
    free(args);
    for (;;) {
        if (generation != InterlockedCompareExchange(&g_playback_generation, 0, 0)) {
            break;
        }
        if (InterlockedCompareExchange(&g_playing, 1, 1) == 0) {
            break;
        }
        if (g_source_voice) {
            XAUDIO2_VOICE_STATE state;
            IXAudio2SourceVoice_GetState(g_source_voice, &state, 0);
            if (state.BuffersQueued == 0) {
                break;
            }
        }
        Sleep(50);
    }
    if (generation == InterlockedCompareExchange(&g_playback_generation, 0, 0)) {
        PostMessageW(g_notify_hwnd, WM_APP_PLAYBACK_DONE, 0, 0);
    }
    return 0;
}

BOOL audio_system_init(HWND hwnd, wchar_t **error_text) {
    if (error_text) {
        *error_text = NULL;
    }
    g_notify_hwnd = hwnd;
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_LITE);
    if (FAILED(hr)) {
        if (error_text) {
            *error_text = format_hresult_error(hr);
        }
        return FALSE;
    }
    hr = XAudio2Create(&g_xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        if (error_text) {
            *error_text = format_hresult_error(hr);
        }
        return FALSE;
    }
    hr = IXAudio2_CreateMasteringVoice(g_xaudio, &g_master_voice, XAUDIO2_DEFAULT_CHANNELS,
                                       XAUDIO2_DEFAULT_SAMPLERATE, 0, NULL, NULL, AudioCategory_Other);
    if (FAILED(hr)) {
        if (error_text) {
            *error_text = format_hresult_error(hr);
        }
        return FALSE;
    }
    return TRUE;
}

void audio_stop(void) {
    InterlockedIncrement(&g_playback_generation);
    InterlockedExchange(&g_playing, 0);
    if (g_source_voice) {
        IXAudio2SourceVoice_Stop(g_source_voice, 0, XAUDIO2_COMMIT_NOW);
        IXAudio2SourceVoice_FlushSourceBuffers(g_source_voice);
        IXAudio2SourceVoice_DestroyVoice(g_source_voice);
        g_source_voice = NULL;
    }
    if (g_monitor_thread) {
        CloseHandle(g_monitor_thread);
        g_monitor_thread = NULL;
    }
    free(g_playback_pcm);
    g_playback_pcm = NULL;
    g_playback_pcm_size = 0;
}

void audio_system_shutdown(void) {
    audio_stop();
    if (g_master_voice) {
        IXAudio2MasteringVoice_DestroyVoice(g_master_voice);
        g_master_voice = NULL;
    }
    if (g_xaudio) {
        IXAudio2_Release(g_xaudio);
        g_xaudio = NULL;
    }
    MFShutdown();
}

BOOL audio_is_playing(void) {
    return InterlockedCompareExchange(&g_playing, 1, 1) != 0;
}

static BOOL decode_base64(const char *base64, BYTE **data, DWORD *size) {
    *data = NULL;
    *size = 0;
    DWORD bytes = 0;
    if (!CryptStringToBinaryA(base64, 0, CRYPT_STRING_BASE64, NULL, &bytes, NULL, NULL)) {
        return FALSE;
    }
    BYTE *buf = (BYTE *)malloc(bytes);
    if (!buf) {
        return FALSE;
    }
    if (!CryptStringToBinaryA(base64, 0, CRYPT_STRING_BASE64, buf, &bytes, NULL, NULL)) {
        free(buf);
        return FALSE;
    }
    *data = buf;
    *size = bytes;
    return TRUE;
}

BOOL audio_parse_response(const char *response_text, BYTE **audio_data, DWORD *audio_size,
                          wchar_t **final_preview, wchar_t **error_text) {
    *audio_data = NULL;
    *audio_size = 0;
    *final_preview = NULL;
    if (error_text) {
        *error_text = NULL;
    }
    cJSON *root = cJSON_Parse(response_text ? response_text : "");
    if (!root) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"响应不是合法 JSON。");
        }
        return FALSE;
    }
    const cJSON *choices = cJSON_GetObjectItemCaseSensitive(root, "choices");
    const cJSON *first = cJSON_IsArray(choices) ? cJSON_GetArrayItem(choices, 0) : NULL;
    const cJSON *message = first ? json_get_object(first, "message") : NULL;
    const cJSON *audio = message ? json_get_object(message, "audio") : NULL;
    const char *preview = message ? json_get_string_value(message, "final_text_preview") : NULL;
    const char *data = audio ? json_get_string_value(audio, "data") : NULL;
    if (preview) {
        *final_preview = utf8_to_utf16(preview);
    } else {
        *final_preview = wcs_dup_or_empty(L"");
    }
    BOOL ok = FALSE;
    if (!data || !decode_base64(data, audio_data, audio_size)) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"响应中缺少可解码的 audio.data。");
        }
    } else {
        ok = TRUE;
    }
    cJSON_Delete(root);
    return ok;
}

static BOOL looks_like_wav(const BYTE *data, DWORD size) {
    return size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WAVE", 4) == 0;
}

static BOOL wav_extract_pcm_copy(const BYTE *data, DWORD size, BYTE **pcm, DWORD *pcm_size) {
    if (!looks_like_wav(data, size)) {
        return FALSE;
    }
    DWORD pos = 12;
    WORD format_tag = 0;
    WORD channels = 0;
    DWORD sample_rate = 0;
    WORD bits = 0;
    const BYTE *data_chunk = NULL;
    DWORD data_size = 0;
    while (pos + 8 <= size) {
        DWORD chunk_size = *(const DWORD *)(data + pos + 4);
        const BYTE *chunk = data + pos + 8;
        if (memcmp(data + pos, "fmt ", 4) == 0 && chunk_size >= 16 && pos + 8 + chunk_size <= size) {
            format_tag = *(const WORD *)(chunk);
            channels = *(const WORD *)(chunk + 2);
            sample_rate = *(const DWORD *)(chunk + 4);
            bits = *(const WORD *)(chunk + 14);
        } else if (memcmp(data + pos, "data", 4) == 0 && pos + 8 + chunk_size <= size) {
            data_chunk = chunk;
            data_size = chunk_size;
        }
        pos += 8 + chunk_size + (chunk_size & 1);
    }
    if (!data_chunk || format_tag != WAVE_FORMAT_PCM || channels != PCM_CHANNELS ||
        sample_rate != PCM_SAMPLE_RATE || bits != PCM_BITS_PER_SAMPLE) {
        return FALSE;
    }
    BYTE *copy = (BYTE *)malloc(data_size);
    if (!copy) {
        return FALSE;
    }
    memcpy(copy, data_chunk, data_size);
    *pcm = copy;
    *pcm_size = data_size;
    return TRUE;
}

static BOOL decode_with_media_foundation(const BYTE *data, DWORD size, BYTE **pcm, DWORD *pcm_size, wchar_t **error_text) {
    *pcm = NULL;
    *pcm_size = 0;
    IStream *stream = SHCreateMemStream(data, size);
    if (!stream) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"无法创建内存音频流。");
        }
        return FALSE;
    }
    IMFByteStream *byte_stream = NULL;
    IMFSourceReader *reader = NULL;
    IMFMediaType *media_type = NULL;
    HRESULT hr = MFCreateMFByteStreamOnStream(stream, &byte_stream);
    if (SUCCEEDED(hr)) {
        hr = MFCreateSourceReaderFromByteStream(byte_stream, NULL, &reader);
    }
    if (SUCCEEDED(hr)) {
        hr = MFCreateMediaType(&media_type);
    }
    if (SUCCEEDED(hr)) {
        IMFMediaType_SetGUID(media_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
        IMFMediaType_SetGUID(media_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
        IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_NUM_CHANNELS, PCM_CHANNELS);
        IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, PCM_SAMPLE_RATE);
        IMFMediaType_SetUINT32(media_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, PCM_BITS_PER_SAMPLE);
        hr = IMFSourceReader_SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, media_type);
    }
    BYTE *buffer = NULL;
    DWORD used = 0;
    DWORD cap = 0;
    while (SUCCEEDED(hr)) {
        DWORD flags = 0;
        IMFSample *sample = NULL;
        hr = IMFSourceReader_ReadSample(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &sample);
        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM)) {
            if (sample) {
                IMFSample_Release(sample);
            }
            break;
        }
        if (sample) {
            IMFMediaBuffer *media_buffer = NULL;
            hr = IMFSample_ConvertToContiguousBuffer(sample, &media_buffer);
            if (SUCCEEDED(hr)) {
                BYTE *src = NULL;
                DWORD max_len = 0;
                DWORD current_len = 0;
                hr = IMFMediaBuffer_Lock(media_buffer, &src, &max_len, &current_len);
                if (SUCCEEDED(hr) && current_len > 0) {
                    if (used + current_len > cap) {
                        DWORD new_cap = cap ? cap * 2 : 65536;
                        while (new_cap < used + current_len) {
                            new_cap *= 2;
                        }
                        BYTE *new_buffer = (BYTE *)realloc(buffer, new_cap);
                        if (!new_buffer) {
                            hr = E_OUTOFMEMORY;
                        } else {
                            buffer = new_buffer;
                            cap = new_cap;
                        }
                    }
                    if (SUCCEEDED(hr)) {
                        memcpy(buffer + used, src, current_len);
                        used += current_len;
                    }
                }
                if (src) {
                    IMFMediaBuffer_Unlock(media_buffer);
                }
                IMFMediaBuffer_Release(media_buffer);
            }
            IMFSample_Release(sample);
        }
    }
    if (media_type) {
        IMFMediaType_Release(media_type);
    }
    if (reader) {
        IMFSourceReader_Release(reader);
    }
    if (byte_stream) {
        IMFByteStream_Release(byte_stream);
    }
    IStream_Release(stream);
    if (FAILED(hr) || used == 0) {
        free(buffer);
        if (error_text) {
            *error_text = FAILED(hr) ? format_hresult_error(hr) : wcs_dup_or_empty(L"音频解码没有产生 PCM 数据。");
        }
        return FALSE;
    }
    *pcm = buffer;
    *pcm_size = used;
    return TRUE;
}

static BOOL convert_to_playback_pcm(const AudioBuffer *buffer, BYTE **pcm, DWORD *pcm_size, wchar_t **error_text) {
    if (_wcsicmp(buffer->format, L"pcm") == 0 || _wcsicmp(buffer->format, L"pcm16") == 0) {
        BYTE *copy = (BYTE *)malloc(buffer->size);
        if (!copy) {
            return FALSE;
        }
        memcpy(copy, buffer->data, buffer->size);
        *pcm = copy;
        *pcm_size = buffer->size;
        return TRUE;
    }
    if (wav_extract_pcm_copy(buffer->data, buffer->size, pcm, pcm_size)) {
        return TRUE;
    }
    return decode_with_media_foundation(buffer->data, buffer->size, pcm, pcm_size, error_text);
}

BOOL audio_play(const AudioBuffer *buffer, wchar_t **error_text) {
    if (error_text) {
        *error_text = NULL;
    }
    if (!buffer || !buffer->data || buffer->size == 0 || !g_xaudio) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"没有可播放的音频。");
        }
        return FALSE;
    }
    audio_stop();
    BYTE *pcm = NULL;
    DWORD pcm_size = 0;
    if (!convert_to_playback_pcm(buffer, &pcm, &pcm_size, error_text)) {
        return FALSE;
    }
    WAVEFORMATEX fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = PCM_CHANNELS;
    fmt.nSamplesPerSec = PCM_SAMPLE_RATE;
    fmt.wBitsPerSample = PCM_BITS_PER_SAMPLE;
    fmt.nBlockAlign = PCM_BLOCK_ALIGN;
    fmt.nAvgBytesPerSec = PCM_AVG_BYTES_PER_SEC;
    HRESULT hr = IXAudio2_CreateSourceVoice(g_xaudio, &g_source_voice, &fmt, 0, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
    if (FAILED(hr)) {
        free(pcm);
        if (error_text) {
            *error_text = format_hresult_error(hr);
        }
        return FALSE;
    }
    XAUDIO2_BUFFER xa_buffer;
    memset(&xa_buffer, 0, sizeof(xa_buffer));
    xa_buffer.AudioBytes = pcm_size;
    xa_buffer.pAudioData = pcm;
    xa_buffer.Flags = XAUDIO2_END_OF_STREAM;
    hr = IXAudio2SourceVoice_SubmitSourceBuffer(g_source_voice, &xa_buffer, NULL);
    if (SUCCEEDED(hr)) {
        hr = IXAudio2SourceVoice_Start(g_source_voice, 0, XAUDIO2_COMMIT_NOW);
    }
    if (FAILED(hr)) {
        IXAudio2SourceVoice_DestroyVoice(g_source_voice);
        g_source_voice = NULL;
        free(pcm);
        if (error_text) {
            *error_text = format_hresult_error(hr);
        }
        return FALSE;
    }
    g_playback_pcm = pcm;
    g_playback_pcm_size = pcm_size;
    LONG generation = InterlockedIncrement(&g_playback_generation);
    InterlockedExchange(&g_playing, 1);
    PlaybackMonitorArgs *args = (PlaybackMonitorArgs *)calloc(1, sizeof(PlaybackMonitorArgs));
    if (args) {
        args->generation = generation;
    }
    g_monitor_thread = CreateThread(NULL, 0, playback_monitor_thread, args, 0, NULL);
    if (!g_monitor_thread) {
        free(args);
    }
    if (g_monitor_thread) {
        CloseHandle(g_monitor_thread);
        g_monitor_thread = NULL;
    }
    return TRUE;
}

static BOOL write_wav_file(const wchar_t *path, const BYTE *pcm, DWORD pcm_size) {
    DWORD riff_size = 36 + pcm_size;
    BYTE header[44];
    memset(header, 0, sizeof(header));
    memcpy(header, "RIFF", 4);
    memcpy(header + 4, &riff_size, 4);
    memcpy(header + 8, "WAVEfmt ", 8);
    DWORD fmt_size = 16;
    WORD format = WAVE_FORMAT_PCM;
    WORD channels = PCM_CHANNELS;
    DWORD sample_rate = PCM_SAMPLE_RATE;
    DWORD avg = PCM_AVG_BYTES_PER_SEC;
    WORD align = PCM_BLOCK_ALIGN;
    WORD bits = PCM_BITS_PER_SAMPLE;
    memcpy(header + 16, &fmt_size, 4);
    memcpy(header + 20, &format, 2);
    memcpy(header + 22, &channels, 2);
    memcpy(header + 24, &sample_rate, 4);
    memcpy(header + 28, &avg, 4);
    memcpy(header + 32, &align, 2);
    memcpy(header + 34, &bits, 2);
    memcpy(header + 36, "data", 4);
    memcpy(header + 40, &pcm_size, 4);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    DWORD written = 0;
    BOOL ok = WriteFile(file, header, sizeof(header), &written, NULL) && written == sizeof(header);
    if (ok) {
        ok = WriteFile(file, pcm, pcm_size, &written, NULL) && written == pcm_size;
    }
    CloseHandle(file);
    return ok;
}

static BOOL write_mp3_file(const wchar_t *path, const BYTE *pcm, DWORD pcm_size, wchar_t **error_text) {
    IMFSinkWriter *writer = NULL;
    IMFMediaType *output_type = NULL;
    IMFMediaType *input_type = NULL;
    IMFSample *sample = NULL;
    IMFMediaBuffer *buffer = NULL;
    DWORD stream = 0;
    HRESULT hr = MFCreateSinkWriterFromURL(path, NULL, NULL, &writer);
    if (SUCCEEDED(hr)) {
        hr = MFCreateMediaType(&output_type);
    }
    if (SUCCEEDED(hr)) {
        IMFMediaType_SetGUID(output_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
        IMFMediaType_SetGUID(output_type, &MF_MT_SUBTYPE, &MFAudioFormat_MP3);
        IMFMediaType_SetUINT32(output_type, &MF_MT_AUDIO_NUM_CHANNELS, PCM_CHANNELS);
        IMFMediaType_SetUINT32(output_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100);
        IMFMediaType_SetUINT32(output_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 16000);
        hr = IMFSinkWriter_AddStream(writer, output_type, &stream);
    }
    if (SUCCEEDED(hr)) {
        hr = MFCreateMediaType(&input_type);
    }
    if (SUCCEEDED(hr)) {
        IMFMediaType_SetGUID(input_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
        IMFMediaType_SetGUID(input_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
        IMFMediaType_SetUINT32(input_type, &MF_MT_AUDIO_NUM_CHANNELS, PCM_CHANNELS);
        IMFMediaType_SetUINT32(input_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, PCM_SAMPLE_RATE);
        IMFMediaType_SetUINT32(input_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, PCM_BITS_PER_SAMPLE);
        IMFMediaType_SetUINT32(input_type, &MF_MT_AUDIO_BLOCK_ALIGNMENT, PCM_BLOCK_ALIGN);
        IMFMediaType_SetUINT32(input_type, &MF_MT_AUDIO_AVG_BYTES_PER_SECOND, PCM_AVG_BYTES_PER_SEC);
        hr = IMFSinkWriter_SetInputMediaType(writer, stream, input_type, NULL);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSinkWriter_BeginWriting(writer);
    }
    if (SUCCEEDED(hr)) {
        hr = MFCreateSample(&sample);
    }
    if (SUCCEEDED(hr)) {
        hr = MFCreateMemoryBuffer(pcm_size, &buffer);
    }
    if (SUCCEEDED(hr)) {
        BYTE *dst = NULL;
        DWORD max_len = 0;
        DWORD current_len = 0;
        hr = IMFMediaBuffer_Lock(buffer, &dst, &max_len, &current_len);
        if (SUCCEEDED(hr)) {
            memcpy(dst, pcm, pcm_size);
            IMFMediaBuffer_Unlock(buffer);
            hr = IMFMediaBuffer_SetCurrentLength(buffer, pcm_size);
        }
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSample_AddBuffer(sample, buffer);
    }
    if (SUCCEEDED(hr)) {
        LONGLONG duration = ((LONGLONG)(pcm_size / PCM_BLOCK_ALIGN) * 10000000LL) / PCM_SAMPLE_RATE;
        IMFSample_SetSampleTime(sample, 0);
        IMFSample_SetSampleDuration(sample, duration);
        hr = IMFSinkWriter_WriteSample(writer, stream, sample);
    }
    if (SUCCEEDED(hr)) {
        hr = IMFSinkWriter_Finalize(writer);
    }
    if (buffer) {
        IMFMediaBuffer_Release(buffer);
    }
    if (sample) {
        IMFSample_Release(sample);
    }
    if (input_type) {
        IMFMediaType_Release(input_type);
    }
    if (output_type) {
        IMFMediaType_Release(output_type);
    }
    if (writer) {
        IMFSinkWriter_Release(writer);
    }
    if (FAILED(hr)) {
        if (error_text) {
            *error_text = format_hresult_error(hr);
        }
        return FALSE;
    }
    return TRUE;
}

BOOL audio_save_to_file(const AudioBuffer *buffer, const wchar_t *path, wchar_t **error_text) {
    if (error_text) {
        *error_text = NULL;
    }
    if (!buffer || !buffer->data || buffer->size == 0) {
        if (error_text) {
            *error_text = wcs_dup_or_empty(L"没有可保存的音频。");
        }
        return FALSE;
    }
    const wchar_t *ext = wcsrchr(path, L'.');
    if (ext && _wcsicmp(ext, L".pcm") == 0) {
        if (_wcsicmp(buffer->format, L"pcm") == 0 || _wcsicmp(buffer->format, L"pcm16") == 0) {
            return write_file_bytes(path, buffer->data, buffer->size);
        }
        BYTE *pcm = NULL;
        DWORD pcm_size = 0;
        BOOL ok = convert_to_playback_pcm(buffer, &pcm, &pcm_size, error_text);
        if (ok) {
            ok = write_file_bytes(path, pcm, pcm_size);
        }
        free(pcm);
        return ok;
    }
    if (ext && _wcsicmp(ext, L".wav") == 0) {
        if (looks_like_wav(buffer->data, buffer->size)) {
            return write_file_bytes(path, buffer->data, buffer->size);
        }
        BYTE *pcm = NULL;
        DWORD pcm_size = 0;
        BOOL ok = convert_to_playback_pcm(buffer, &pcm, &pcm_size, error_text);
        if (ok) {
            ok = write_wav_file(path, pcm, pcm_size);
        }
        free(pcm);
        return ok;
    }
    if (ext && _wcsicmp(ext, L".mp3") == 0 && _wcsicmp(buffer->format, L"mp3") == 0) {
        return write_file_bytes(path, buffer->data, buffer->size);
    }
    if (ext && _wcsicmp(ext, L".mp3") == 0) {
        BYTE *pcm = NULL;
        DWORD pcm_size = 0;
        BOOL ok = convert_to_playback_pcm(buffer, &pcm, &pcm_size, error_text);
        if (ok) {
            ok = write_mp3_file(path, pcm, pcm_size, error_text);
        }
        free(pcm);
        return ok;
    }
    return write_file_bytes(path, buffer->data, buffer->size);
}
