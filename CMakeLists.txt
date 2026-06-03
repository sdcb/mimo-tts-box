cmake_minimum_required(VERSION 3.21)

project(MimoTTSBox C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  add_compile_options(/W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS /DUNICODE /D_UNICODE)
endif()

add_executable(MimoTTSBox WIN32
  src/main.c
  src/ui_main.c
  src/ui_controls.c
  src/ui_dialogs.c
  src/ui_history_list.c
  src/ui_layout.c
  src/ui_metrics.c
  src/ui_request_flow.c
  src/ui_response_view.c
  src/config.c
  src/request_builder.c
  src/http_client.c
  src/history_store.c
  src/audio.c
  src/json_helpers.c
  src/win32_helpers.c
  third_party/cjson/cJSON.c
  res/MimoTTSBox.rc
)

target_include_directories(MimoTTSBox PRIVATE
  src
  third_party/cjson
  res
)

target_link_libraries(MimoTTSBox PRIVATE
  comctl32
  winhttp
  crypt32
  mfplat
  mfreadwrite
  mfuuid
  xaudio2
  ole32
  uuid
  shlwapi
  propsys
)

set_target_properties(MimoTTSBox PROPERTIES
  OUTPUT_NAME "MimoTTSBox"
)

if(MSVC)
  target_link_options(MimoTTSBox PRIVATE /MANIFEST:NO)
endif()

add_executable(MimoTTSBoxSelfTest
  tests/audio_selftest.c
  src/audio.c
  src/json_helpers.c
  src/win32_helpers.c
  third_party/cjson/cJSON.c
)

target_include_directories(MimoTTSBoxSelfTest PRIVATE
  src
  third_party/cjson
)

target_link_libraries(MimoTTSBoxSelfTest PRIVATE
  crypt32
  mfplat
  mfreadwrite
  mfuuid
  xaudio2
  ole32
  uuid
  shlwapi
  propsys
)
