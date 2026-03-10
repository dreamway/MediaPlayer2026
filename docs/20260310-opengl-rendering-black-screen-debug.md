# OpenGL 渲染黑屏问题调试记录

**日期**: 2026-03-10
**问题**: 视频播放时黑屏，OpenGL 渲染错误
**状态**: 已修复 ✅

---

## 1. 编译操作

```bash
# 编译项目
cmake --build build

# 运行应用（带命令行参数）
./build/WZMediaPlayer.app/Contents/MacOS/WZMediaPlayer testing/video/LG.mp4

# 杀掉所有进程
pkill -f WZMediaPlayer
```

---

## 2. 日志存储位置

| 类型 | 路径 |
|------|------|
| 运行时日志 | stdout/stderr |
| 应用日志目录 | `./build/WZMediaPlayer.app/Contents/MacOS/logs/` |
| 临时测试日志 | `/tmp/wzplayer_test.log` |

**获取日志命令**:
```bash
# 启动并捕获日志
./build/WZMediaPlayer.app/Contents/MacOS/WZMediaPlayer testing/video/LG.mp4 > /tmp/wzplayer_test.log 2>&1 &

# 查看渲染相关日志
cat /tmp/wzplayer_test.log | grep -E "(paintGL|OpenGL error|texture|shader)"

# 查看错误
cat /tmp/wzplayer_test.log | grep -E "(error|ERROR)"
```

---

## 3. 主要分析方法

### 3.1 OpenGL 错误诊断

在关键 OpenGL 调用后检查 `glGetError()`:

```cpp
GLenum glError = glGetError();
if (glError != GL_NO_ERROR) {
    // GL_INVALID_ENUM (0x500) - 枚举参数无效
    // GL_INVALID_OPERATION (0x502) - 操作状态无效
}
```

### 3.2 关键参数日志

```cpp
logger->info("numPlanes: {}, viewport: {}x{}, textures: {}/{}/{}",
    numPlanes, viewport[2], viewport[3], textures[1], textures[2], textures[3]);
```

### 3.3 问题定位流程

1. 检查 shader 编译/链接是否成功
2. 检查纹理是否正确上传（numPlanes, textures[]）
3. 检查 uniform 是否正确设置
4. 检查 OpenGL 错误码

---

## 4. 已发现的根本原因及修复

### 4.1 GL_INVALID_ENUM (0x500) - 已修复 ✅

**原因**: `GL_LUMINANCE` 在 OpenGL 3.3 Core Profile 中已弃用

**错误代码**:
```cpp
const GLint internalFmt = (bytesMultiplier == 1) ? GL_LUMINANCE : GL_R16;
const GLenum fmt = (bytesMultiplier == 1) ? GL_LUMINANCE : GL_RED;
```

**修复**:
```cpp
// OpenGL 3.3 Core Profile 不支持 GL_LUMINANCE，使用 GL_RED/GL_R8 替代
const GLint internalFmt = (bytesMultiplier == 1) ? GL_R8 : GL_R16;
const GLenum fmt = GL_RED;
```

**涉及文件**:
- `StereoOpenGLCommon.cpp:384-386`
- `OpenGLCommon.cpp:400-401`

### 4.2 NV12/YUV420P 纹理采样错误 - 已修复 ✅

**原因**: fragment.glsl 没有区分 NV12 和 YUV420P 格式的 UV 采样方式

**问题**:
- NV12: UV 交织在一个纹理中 (`textureUV.rg`)
- YUV420P: U 和 V 是独立的纹理 (`textureU.r`, `textureV.r`)

**修复**: 在 fragment.glsl 中添加 `iVideoFormat` uniform:
```glsl
uniform int iVideoFormat;  // 0=RGB, 1=NV12, 2=YUV420P

// 根据 iVideoFormat 区分采样方式
if (iVideoFormat == 1) {
    // NV12 格式
    vec2 uv = texture(textureUV, TexCoord).rg;
    u = uv.r;
    v = uv.g;
} else {
    // YUV420P 格式
    u = texture(textureU, TexCoord).r;
    v = texture(textureV, TexCoord).r;
}
```

### 4.3 GL_INVALID_OPERATION (0x502) - 已修复 ✅

**原因**: OpenGL 3.3 Core Profile 不支持直接使用 CPU 端的顶点数据指针，必须使用 VBO

**错误代码位置** (`StereoOpenGLCommon.cpp:509-513`):
```cpp
// 错误：直接使用 CPU 指针
shaderProgramStereo->setAttributeArray(positionYCbCrLoc, verticesYCbCr[verticesIdx], 2);
shaderProgramStereo->setAttributeArray(texCoordYCbCrLoc, texCoordYCbCr, 2);
```

**修复方案**: 使用 VBO (Vertex Buffer Object)

**在 `initializeStereoShader()` 中创建 VBO**:
```cpp
// 创建并绑定 VAO
m_vao.create();
m_vao.bind();

// 创建位置 VBO
glGenBuffers(1, &m_vboPosition);
glBindBuffer(GL_ARRAY_BUFFER, m_vboPosition);
glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, verticesYCbCr[0], GL_DYNAMIC_DRAW);

// 设置位置顶点属性指针（存储在 VAO 中）
glEnableVertexAttribArray(positionYCbCrLoc);
glVertexAttribPointer(positionYCbCrLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

// 创建纹理坐标 VBO
glGenBuffers(1, &m_vboTexCoord);
glBindBuffer(GL_ARRAY_BUFFER, m_vboTexCoord);
glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, texCoordYCbCr, GL_STATIC_DRAW);

// 设置纹理坐标顶点属性指针（存储在 VAO 中）
glEnableVertexAttribArray(texCoordYCbCrLoc);
glVertexAttribPointer(texCoordYCbCrLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

m_vao.release();  // 解绑 VAO，保存所有顶点属性状态
```

**在 `paintGLStereo()` 中使用 VBO**:
```cpp
// 绑定 VAO（顶点属性状态已存储在其中）
m_vao.bind();

// 更新位置 VBO 数据（如果顶点索引改变）
if (lastVerticesIdx != verticesIdx) {
    glBindBuffer(GL_ARRAY_BUFFER, m_vboPosition);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float) * 8, verticesYCbCr[verticesIdx]);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    lastVerticesIdx = verticesIdx;
}

// 绑定 shader 并绘制
shaderProgramStereo->bind();
glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
shaderProgramStereo->release();

m_vao.release();
```

### 4.4 Sampler Uniform 设置错误 - 已修复 ✅

**原因**: Sampler uniform 设置为 -1 会导致 GL_INVALID_OPERATION 错误

**错误代码**:
```cpp
shaderProgramStereo->setUniformValue("textureU", -1);
shaderProgramStereo->setUniformValue("textureV", -1);
```

**修复**: 设置为有效的纹理单元索引
```cpp
if (numPlanes == 2) {
    shaderProgramStereo->setUniformValue("textureUV", 1);
    shaderProgramStereo->setUniformValue("textureU", 1);  // 有效值
    shaderProgramStereo->setUniformValue("textureV", 2);  // 有效值
}
```

---

## 5. 已完成的修改

| 文件 | 修改内容 |
|------|----------|
| `Shader/fragment.glsl` | 添加 `iVideoFormat` uniform，区分 NV12/YUV420P 格式的 UV 采样方式 |
| `StereoOpenGLCommon.cpp` | 1. 设置 `iVideoFormat` uniform；2. 将 `GL_LUMINANCE` 改为 `GL_R8`/`GL_RED`；3. 实现 VBO 替代直接指针；4. 修复 sampler uniform 设置 |
| `StereoOpenGLCommon.hpp` | 添加 VBO 成员变量 (`m_vboPosition`, `m_vboTexCoord`, `m_vboInitialized`) |
| `OpenGLCommon.cpp` | 将 `GL_LUMINANCE` 改为 `GL_R8`/`GL_RED` |

---

## 6. 关键文件参考

| 文件路径 | 用途 |
|---------|------|
| `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.cpp` | 3D 渲染主逻辑，`paintGLStereo()` 是渲染入口 |
| `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.hpp` | 类定义，包含 VAO/VBO 成员 |
| `WZMediaPlay/videoDecoder/opengl/OpenGLCommon.cpp` | 基类渲染逻辑 |
| `WZMediaPlay/Shader/fragment.glsl` | Fragment shader，YUV 到 RGB 转换 |
| `WZMediaPlay/Shader/vertex.glsl` | Vertex shader |
| `WZMediaPlay/videoDecoder/opengl/OpenGLVertices.hpp` | 顶点数据定义 |

---

## 7. 测试资源

```bash
testing/video/LG.mp4    # 主测试视频 (H.264, 24fps, 约42秒)
```

---

## 8. 相关文档

- `CLAUDE.md` - 项目构建和代码规范
- `docs/cursor/FULL_BUG_REGISTRY.md` - Bug 注册表
- `docs/TODO_CURRENT.md` - 当前任务列表