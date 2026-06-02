# Mimo TTS Box

Mimo TTS Box 是一个使用纯 C 语言和 Win32 API 编写的小米 Mimo TTS API 桌面小工具。它可以配置 API 地址和 API Key，发送文本转语音请求，并支持播放、停止和保存返回的音频。

开源项目地址：https://github.com/sdcb/mimo-tts-box

## 功能简介

- 调用小米 Mimo TTS chat-completions API 进行文本转语音。
- 支持配置 URL、API Key、音色、输出格式和文本预览优化选项。
- 支持请求历史记录，便于查看之前的请求和响应。
- 支持播放、停止和保存响应音频。

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
