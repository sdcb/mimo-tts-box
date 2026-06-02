# Agent Notes

This repository is a plain C Win32 desktop app for testing the Mimo TTS chat-completions API. Keep changes small and aligned with the existing module boundaries.

## Project Layout

- `src/main.c`: process startup, common-controls init, COM init, main message loop, accelerator table.
- `src/ui_main.c/.h`: all Win32 UI, layout, dialogs, history activation, send/play/save commands.
- `src/config.c/.h`: config load/save. API keys are stored in `appsettings.json` using DPAPI.
- `src/request_builder.c/.h`: builds the JSON request body from `RequestSettings`.
- `src/http_client.c/.h`: WinHTTP POST logic.
- `src/history_store.c/.h`: `AppData/<timestamp>/` history persistence.
- `src/audio.c/.h`: response audio parsing, media conversion, XAudio2 playback, file save.
- `src/json_helpers.c/.h` and `third_party/cjson/`: JSON helpers and cJSON.
- `res/`: icon, menu/resource script, manifest.
- `tests/audio_selftest.c`: offline response parsing/conversion/playback smoke test.
- `specs/`: API request/response examples and implementation spec.

Generated/runtime files are intentionally ignored: `build/`, `build-release/`, `appsettings.json`, `.env`, `AppData/`, audio outputs, PDB/ILK/EXE/etc.

## Build

Use a separate Release directory. Do not reuse a Debug `build` directory and expect `--config Release` to change it when using Ninja single-config generators.

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" && cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release'
```

Current CMake uses static MSVC runtime:

- Debug: `/MTd`, `/Od`, `/Zi`, `/RTC1`, `/debug`, `/INCREMENTAL`
- Release: `/MT`, `/O2`, `/DNDEBUG`, `/INCREMENTAL:NO`

A Debug exe around 1.5 MB is expected. A Release exe should be roughly a few hundred KB.

Run the offline smoke test after changes touching audio, JSON, build flags, or shared helpers:

```powershell
$out = Join-Path (Resolve-Path .).Path 'build-release\selftest-audio'
if (Test-Path $out) { Remove-Item -LiteralPath $out -Recurse -Force }
New-Item -ItemType Directory -Path $out | Out-Null
& build-release\MimoTTSBoxSelfTest.exe specs\resp-examples\pcm.base64.example specs\resp-examples\wav.base64.example specs\resp-examples\mp3.base64.example $out
```

Expected lines include `pcm ok`, `wav ok`, `mp3 ok`, and `playback smoke ok`.

## Test Project

`MimoTTSBoxSelfTest` is a small console test target, not a full unit-test framework. It exists to exercise the most failure-prone non-UI path without requiring a live API key.

Source:

- `tests/audio_selftest.c`

It links only the audio/JSON/helper stack:

- `src/audio.c`
- `src/json_helpers.c`
- `src/win32_helpers.c`
- `third_party/cjson/cJSON.c`

It intentionally does not link the UI, config, WinHTTP request code, or history store. Keep it that way unless there is a concrete need; it should remain a fast offline smoke test.

Inputs:

- `specs/resp-examples/pcm.base64.example`
- `specs/resp-examples/wav.base64.example`
- `specs/resp-examples/mp3.base64.example`
- an output directory path

What it verifies:

- Wraps the base64 examples into response-shaped JSON.
- Calls `audio_parse_response`.
- Confirms PCM/WAV/MP3 payloads decode.
- Writes decoded outputs into the supplied directory as `.pcm`, `.wav`, and `.mp3`.
- Runs an XAudio2 playback smoke path: initialize audio, start playback, then stop.

What it does not verify:

- Real network/API behavior.
- UI layout or automation.
- DPAPI config persistence.
- History directory persistence.
- Human-audible audio quality beyond the playback path not failing immediately.

When adding or changing response parsing, media conversion, cJSON helpers, XAudio2 setup, or build/link flags, run this target before reporting completion. If a change is only UI layout, it is still cheap enough to run, but visual verification is more important.

## Secrets

The repo may contain a local `.env` with a real API key. Do not read or print it unless explicitly required. Prefer testing no-key flows or use DPAPI-protected `appsettings.json` only when the user explicitly permits real API calls.

Never leave `appsettings.json` behind after temporary secret-based testing unless the user asks for that state.

## Win32 Resource And Manifest Notes

`res/MimoTTSBox.rc` embeds `res/app.manifest` as resource `1 RT_MANIFEST`. CMake disables the linker-generated manifest with `/MANIFEST:NO`; keep that or the embedded manifest can be replaced/merged unexpectedly.

The manifest is important:

- Common Controls v6 for modern controls.
- `dpiAware=true` and `dpiAwareness=system` for System DPI awareness. This project intentionally does not use PerMonitorV2.

When changing `res/app.manifest`, Ninja may not always rebuild the RC resource just because the included manifest changed. If manifest verification looks stale, run:

```powershell
cmake --build build-release --target clean
cmake --build build-release
```

Verify the embedded manifest with:

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul && mt.exe -inputresource:build-release\MimoTTSBox.exe;#1 -out:%TEMP%\mimo_manifest.xml'
Select-String -Path $env:TEMP\mimo_manifest.xml -Pattern 'Microsoft.Windows.Common-Controls|dpiAware|dpiAwareness'
```

Resource strings in the RC file use direct UTF-8 Chinese with:

```rc
#pragma code_page(65001)
POPUP L"文件(&F)"
```

The `L"..."` matters. Earlier escaped narrow resource strings produced mojibake when read back through `GetMenuStringW`.

## UI Notes

This is an immediate Win32 UI, not a dialog resource UI. Do not assume dialog mnemonics work automatically for arbitrary child controls.

- `Alt+S` is handled by both an accelerator in `main.c` and a child-control subclass in `ui_main.c`; this prevents focus in edit controls from turning `Alt+S` into a system beep.
- Main-window child controls are created through `make_child`, which sets the app font and installs the subclass.
- Config/about dialogs use `make_dialog_child`, which scales coordinates by DPI and sets `Segoe UI`.
- Static labels use `WM_CTLCOLORSTATIC`/`WM_CTLCOLORBTN` where needed so labels render on normal white window background instead of gray patches.
- Splitters keep a wider hit target for usability but paint as a thin line. Keep hit size and visual line size separate.
- Multiline edit controls should not permanently show vertical scrollbars. `update_multiline_scrollbar` toggles `WS_VSCROLL` based on content and available space.
- The response panel layout is: audio buttons above the response text box, status line below it.
- The history list is single-select. Sending a request inserts the new row at the top and selects/focuses it immediately.

For screenshots, hidden `PrintWindow` can produce black images. Use a visible window and close modal dialogs with `PostMessage`, not blocking `SendMessage`, because modal loops can deadlock automation.

## DPI Notes

The app is System DPI aware. Layout constants are scaled with `scale_for_dpi`. If adding new fixed pixel dimensions, scale them with the window/dialog DPI rather than using raw 96-DPI values.

The app does not handle runtime monitor DPI changes. That is intentional for now because PerMonitorV2 is out of scope.

## Time And History Display

History directory names are stable timestamps like `YYYYMMDD_HHMMSS_fff`. Prefer deriving display time from the directory name rather than rewriting stored metadata.

Request-time display rules:

- Within 24 hours: relative time such as `刚刚`, `20分钟前`, `3小时前`.
- Yesterday/the day before: `昨天 HH:mm` / `前天 HH:mm`.
- Older: `yyyy-MM-dd HH:mm`.
- Do not show `.fff` milliseconds in the list.

Response status line uses elapsed duration, not the completion timestamp. Milliseconds are formatted with thousands separators and two decimals, similar to C# `ToString("N")`, e.g. `3,265.00 ms`.

## Testing Tips

Useful UI checks:

- Use `GetMenuStringW` to verify resource menu strings are real Chinese.
- Use `mt.exe -inputresource:...;#1` to verify comctl32 v6 and DPI manifest.
- For config dialogs, enumerate windows by class name (`MimoTTSBoxConfigWindow`) instead of relying only on title.
- For modal dialogs, interact with `PostMessage` where possible; `SendMessage` can block if it opens another modal loop.

If automation creates temporary working directories, remove them and verify `appsettings.json` was not created in the repo.
