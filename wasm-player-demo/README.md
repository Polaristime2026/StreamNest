# wasm-player-demo

浏览器内置播放器 MVP：

- 解码：C++ + FFmpeg（Emscripten -> WASM）
- 视频：WebGL2，YUV420P 三纹理 shader 转 RGB
- 音频：WebAudio + AudioWorklet + SharedArrayBuffer ring buffer
- 输入：`fetch` 流式读取 MP4 chunk，喂给 WASM

## 目录

```text
wasm-player-demo/
  README.md
  package.json
  vite.config.ts
  public/
    assets/sample.mp4
    worklets/pcm-worklet.js
    wasm/               # build_wasm.sh 输出
  src/
    index.html
    main.ts
    player/
      Player.ts
      gl/
        YuvRenderer.ts
        shaders.ts
      audio/
        AudioEngine.ts
        RingBuffer.ts
  cpp/
    CMakeLists.txt
    include/
      player_api.h
      ffmpeg_wrap.h
    src/
      player_api.cpp
      ring_buffer.h
      ring_buffer.cpp
      ffmpeg_wrap.cpp
  scripts/
    build_wasm.sh
```

## 前置环境

1. Node.js 18+
2. `emsdk`（提供 `em++`）
3. 已交叉编译好的 FFmpeg wasm 静态库（`avformat/avcodec/avutil/swresample/swscale`）

> 本仓库不内置 FFmpeg 二进制，请提供 `FFMPEG_WASM_ROOT`（包含 `include/` 和 `lib/`）。

## 构建与运行

1. 安装前端依赖：

```bash
npm install
```

2. 准备视频样本：

- 将你的 H.264/AAC MP4 放到：`public/assets/sample.mp4`
- 仓库中的 `sample.mp4` 为空占位文件，请替换为真实样本。

3. 编译 WASM：

```bash
source /path/to/emsdk/emsdk_env.sh
export FFMPEG_WASM_ROOT=/abs/path/to/ffmpeg-wasm-prefix
npm run build:wasm
```

4. 启动开发服务器：

```bash
npm run dev
```

5. 浏览器打开 `http://localhost:5173`。

## 功能说明

- 播放/暂停
- 进度条（粗略 seek：`seek + flush`）
- 音量控制
- 倍速 UI（MVP 阶段主要作用于渲染节奏，音频变速可后续接入）

## SharedArrayBuffer 与线程说明

本项目音频路径依赖 `SharedArrayBuffer`。`vite.config.ts` 已设置：

- `Cross-Origin-Opener-Policy: same-origin`
- `Cross-Origin-Embedder-Policy: require-corp`

如果后续启用 Emscripten pthreads，还需要继续保持 COOP/COEP，并在 HTTPS/localhost 下运行。

## 当前实现边界（MVP）

- 演示输入：MP4(H.264/AAC)
- 解码线程：单线程
- seek：近似定位后清空队列，不做精准 A/V 对齐

## 可扩展方向

1. 解码搬到 Worker（主线程仅渲染/UI）
2. 开启 `-msimd128` + FFmpeg SIMD 优化
3. 启用 Emscripten pthreads（多线程解码/滤镜）
4. 输入扩展到 fMP4/HLS（MSE 或自定义 demux）
5. 更完整时钟同步（audio master + video drift correction）
