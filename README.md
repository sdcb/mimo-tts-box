# Mimo TTS Box

Mimo TTS Box 是一个使用纯 C 语言和 Win32 API 编写的小米 Mimo TTS API 桌面小工具。它可以配置 API 地址和 API Key，发送文本转语音请求，并支持播放、停止和保存返回的音频。

开源项目地址：https://github.com/sdcb/mimo-tts-box

## 功能简介

- 调用小米 Mimo TTS chat-completions API 进行文本转语音。
- 支持配置 URL、API Key、音色、输出格式和文本预览优化选项。
- 支持请求历史记录，便于查看之前的请求和响应。
- 支持播放、停止和保存响应音频。

## 技术实现

- 语言与运行时：使用 C11 编写，不依赖 .NET、Electron 或 Qt 等额外运行时。
- 桌面界面：基于 Win32 API 和 Windows Common Controls 构建原生 Windows 桌面窗口、表单控件和历史记录列表。
- HTTP 请求：使用 WinHTTP 发送 HTTPS JSON POST 请求，并处理 API 响应、状态码和错误信息。
- JSON 处理：使用随仓库 vendored 的官方 cJSON 源码解析配置、请求体和响应体，About 中标注了来源哈希。
- 音频处理与播放：使用 Media Foundation (MF) 解码音频，使用 XAudio2 播放音频。
- 安全存储：使用 Windows CryptoAPI/DPAPI 加密保存 API Key，并进行 Base64 编解码。
- 配置与本地数据：使用 JSON 文件保存应用配置和请求历史记录。
- 构建系统：使用 CMake 生成 MSVC 工程，支持本地 Ninja 构建，也支持 CI 中的 NMake 构建。
- 自动化构建：使用 GitHub Actions 在 Windows 2025 + Visual Studio 2026 环境下构建 x64、x86 和 arm64 可执行文件。

## 编译

编译环境：

- Windows
- Visual Studio C++ 构建工具
- CMake 3.21 或更高版本
- Ninja

在仓库根目录执行：

```powershell
cmd /c '"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Auxiliary\Build\vcvars64.bat" && cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-release'
```

编译完成后，程序位于：

```text
build-release\MimoTTSBox.exe
```

## 截图

<img src="https://github.com/user-attachments/assets/cc248c51-de16-4b1d-84de-f530790a6ddc" />
