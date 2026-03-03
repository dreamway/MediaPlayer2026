---
name: "docs-skill"
description: "Query FFmpeg, OpenGL, Qt and other technical documentation. Invoke when user needs to understand API usage, find function descriptions, or query technical details."
---

# Docs Skill - Technical Documentation Query

## Overview

This skill queries technical documentation for WZMediaPlayer project, including FFmpeg, OpenGL, Qt and other core tech stacks.

## Supported Tech Stacks

- **FFmpeg**: Audio/video codec, format processing
- **OpenGL**: Video rendering, shaders
- **Qt**: GUI framework, signals/slots, multithreading
- **OpenAL**: Audio output

## Query Methods

### Method 1: Using Context7 (Recommended)

Query FFmpeg API, Qt signals/slots, OpenGL shaders with Context7.

### Method 2: Query Project Code Examples

Search for FFmpeg usage examples, OpenGL rendering code in project files.

## FFmpeg Quick Reference

### Decode Flow

```c
// 1. Find decoder
AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);

// 2. Alloc context
AVCodecContext *ctx = avcodec_alloc_context3(codec);

// 3. Open decoder
avcodec_open2(ctx, codec, NULL);

// 4. Send packet
avcodec_send_packet(ctx, packet);

// 5. Receive frame
avcodec_receive_frame(ctx, frame);

// 6. Cleanup
avcodec_free_context(&ctx);
```

### Timestamp Conversion

```c
int64_t pts_ms = av_rescale_q(
    pkt->pts,
    fmt_ctx->streams[stream_idx]->time_base,
    AV_TIME_BASE_Q
) / 1000;
```

### Seeking

```c
av_seek_frame(
    fmt_ctx,
    stream_index,
    timestamp,
    AVSEEK_FLAG_BACKWARD | AVSEEK_FLAG_FRAME
);
```

## Qt Quick Reference

### Signal/Slot Connection

```cpp
// Qt5/6 new syntax
connect(sender, &Class::signal, receiver, &Class::slot);

// With lambda
connect(button, &QPushButton::clicked, [this]() {
    // handle click
});
```

### Multithreading

```cpp
// Move to thread
worker->moveToThread(thread);
connect(thread, &QThread::started, worker, &Worker::process);

// Cross-thread signal/slot (auto queued)
connect(worker, &Worker::resultReady, this, &MainWindow::handleResult);
```

## OpenGL Quick Reference

### YUV to RGB Shader

```glsl
// Vertex shader
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
out vec2 TexCoord;
void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}

// Fragment shader
#version 330 core
in vec2 TexCoord;
out vec4 FragColor;
uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;
void main() {
    float y = texture(yTexture, TexCoord).r;
    float u = texture(uTexture, TexCoord).r - 0.5;
    float v = texture(vTexture, TexCoord).r - 0.5;
    float r = y + 1.402 * v;
    float g = y - 0.344 * u - 0.714 * v;
    float b = y + 1.772 * u;
    FragColor = vec4(r, g, b, 1.0);
}
```

## Project Architecture

```
MainWindow (UI)
    ↓
PlayController (Coordinator)
    ├── DemuxerThread (Demux)
    ├── VideoThread (Video decode/render)
    └── AudioThread (Audio decode/output)
```

## Key Class Responsibilities

| Class | Responsibility | Key Methods |
|-------|---------------|-------------|
| PlayController | Playback control, state management, clock sync | seek(), play(), pause() |
| DemuxerThread | Demux, packet distribution | run(), handleSeek() |
| VideoThread | Video decode, render | decodeFrame(), renderFrame() |
| AudioThread | Audio decode, output | decodeFrame(), writeAudio() |
| OpenALAudio | OpenAL wrapper, audio buffer | writeAudio(), getClock() |

## Documentation Resources

- **FFmpeg**: https://ffmpeg.org/documentation.html
- **Qt**: https://doc.qt.io/qt-6/
- **OpenGL**: https://www.khronos.org/opengl/wiki/
- **OpenAL**: https://openal-soft.org/openal-docs/
