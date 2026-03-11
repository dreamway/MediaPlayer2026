# WZMediaPlayer 渲染问题深度分析报告

## 问题描述

**现象**: 渲染不再黑屏，但视频内容不是真实视频图像。

## 已实施的修复（2026-03-11）

### 修改1: Vertex Shader 改为 vec3 格式

**文件**: `WZMediaPlay/Shader/vertex.glsl`

```glsl
#version 330 core
layout (location = 0) in vec3 aPos;      // 改为 vec3（与v1.0.8一致）
layout (location = 1) in vec2 aTexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);        // z 已包含在 aPos 中
    TexCoord = aTexCoord;
}
```

### 修改2: 使用交错数组 + EBO

**文件**: `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.hpp`

```cpp
// VBO (Vertex Buffer Object) - 使用交错数组格式
quint32 m_vbo = 0;

// EBO (Element Buffer Object) - 索引缓冲
quint32 m_ebo = 0;

// 顶点数据（交错数组，20个float = 4顶点 * 5float/顶点）
float m_vertices[20];

// 索引数据（6个索引 = 2三角形 * 3顶点/三角形）
static constexpr unsigned int m_indices[6] = {
    0, 1, 3,  // 第一个三角形
    1, 2, 3   // 第二个三角形
};
```

### 修改3: initializeStereoShader 使用交错数组

**文件**: `WZMediaPlay/videoDecoder/opengl/StereoOpenGLCommon.cpp`

```cpp
// 初始化默认顶点数据（参考v1.0.8 FFmpegView.h:207-212）
// 交错数组格式：每个顶点5个float {x, y, z, tex_x, tex_y}
m_vertices[0]  =  1.0f; m_vertices[1]  =  1.0f; m_vertices[2]  = 0.0f;  // 右上
m_vertices[3]  =  1.0f; m_vertices[4]  = 0.0f;                              // 右上纹理
// ... 其他顶点

// 创建 VBO 和 EBO
glGenBuffers(1, &m_vbo);
glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);

glGenBuffers(1, &m_ebo);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_indices), m_indices, GL_STATIC_DRAW);

// 设置顶点属性指针（步长5个float）
glVertexAttribPointer(positionYCbCrLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
glVertexAttribPointer(texCoordYCbCrLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
```

### 修改4: 绘制使用 glDrawElements

```cpp
// 使用 glDrawElements + EBO 绘制（与v1.0.8一致）
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

### 修改5: updateDynamicVertices 更新交错数组

```cpp
// 交错数组格式：每个顶点5个float {x, y, z, tex_x, tex_y}
// 顶点顺序：右上(0), 右下(1), 左下(2), 左上(3)
float vertices[20] = {
    1.0f + sideViewStrip,  p, 0.0f,     1.0f, 0.0f,  // 右上
    1.0f + sideViewStrip, -p, 0.0f,     1.0f, 1.0f,  // 右下
   -1.0f - sideViewStrip, -p, 0.0f,     0.0f, 1.0f,  // 左下
   -1.0f - sideViewStrip,  p, 0.0f,     0.0f, 0.0f,  // 左上
};
```

---

## 根本原因分析

通过对比v1.0.8和当前版本，发现**多处关键差异**导致渲染内容错误：

---

### 差异1: Vertex Shader 顶点格式不同

**v1.0.8 vertex.glsl:**
```glsl
#version 330 core
layout (location = 0) in vec3 aPos;      // 3D顶点坐标 (x, y, z)
layout (location = 1) in vec2 aTexCoord; // 2D纹理坐标

void main() {
    gl_Position = vec4(aPos, 1.0);  // 直接使用vec3，w=1.0
    TexCoord = aTexCoord;
}
```

**当前版本 vertex.glsl:**
```glsl
#version 150 core
in vec2 aPos;        // 2D顶点坐标 (x, y)
in vec2 aTexCoord;   // 2D纹理坐标

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);  // z=0.0, w=1.0
    TexCoord = aTexCoord;
}
```

**影响**: 虽然当前版本将z设为0.0理论上可行，但shader版本和顶点格式差异可能导致兼容性问题。

---

### 差异2: 顶点数据存储格式不同

**v1.0.8 (FFmpegView.cc:664-670):**
```cpp
// 使用交错数组 (Interleaved Array)
// 每个顶点5个float: {x, y, z, tex_x, tex_y}
float vertices1[20] = {
     1.0,  1.0, 0.0,     1.0, 0.0,  // 顶点0: 位置(1,1,0), 纹理(1,0)
     1.0, -1.0, 0.0,     1.0, 1.0,  // 顶点1: 位置(1,-1,0), 纹理(1,1)
    -1.0, -1.0, 0.0,     0.0, 1.0,  // 顶点2: 位置(-1,-1,0), 纹理(0,1)
    -1.0,  1.0, 0.0,     0.0, 0.0,  // 顶点3: 位置(-1,1,0), 纹理(0,0)
};

// 初始化VBO (FFmpegView.cc:663-670)
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferData(GL_ARRAY_BUFFER, sizeof(vertices1), vertices1, GL_DYNAMIC_DRAW);
glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(glIndices), glIndices, GL_STATIC_DRAW);

// 设置顶点属性指针（步长=5个float）
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);            // 位置: 3个float, 步长5
glEnableVertexAttribArray(0);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); // 纹理: 2个float, 偏移3
glEnableVertexAttribArray(1);
```

**当前版本 (StereoOpenGLCommon.cpp:220-240):**
```cpp
// 使用分离数组 (Separate Arrays)
// 位置VBO: 4个顶点，每个顶点2个float
float vertices[8] = {x0, y0, x1, y1, x2, y2, x3, y3};

// 纹理坐标VBO: 4个顶点，每个顶点2个float
float texCoordYCbCr[8] = {0.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 0.0};

// 初始化VBO
glGenBuffers(1, &m_vboPosition);
glBindBuffer(GL_ARRAY_BUFFER, m_vboPosition);
glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, verticesYCbCr[0], GL_DYNAMIC_DRAW);
glVertexAttribPointer(positionYCbCrLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);  // 步长0

glGenBuffers(1, &m_vboTexCoord);
glBindBuffer(GL_ARRAY_BUFFER, m_vboTexCoord);
glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, texCoordYCbCr, GL_STATIC_DRAW);
glVertexAttribPointer(texCoordYCbCrLoc, 2, GL_FLOAT, GL_FALSE, 0, nullptr);  // 步长0
```

**关键问题**: 分离数组格式本身是正确的，但需要确保：
1. 顶点顺序与纹理坐标顺序对应
2. 顶点位置和纹理坐标的正确映射

---

### 差异3: 绘制方式不同

**v1.0.8:**
```cpp
// 使用EBO (Element Buffer Object) + glDrawElements
unsigned int glIndices[6] = {
    0, 1, 3,  // 第一个三角形
    1, 2, 3   // 第二个三角形
};
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

**当前版本:**
```cpp
// 使用 glDrawArrays + GL_TRIANGLE_STRIP
glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
```

**影响**: 两种方式绘制相同的内容，但顶点顺序要求不同：
- `GL_TRIANGLES + EBO`: 顶点顺序 {0,1,3,1,2,3}
- `GL_TRIANGLE_STRIP`: 顶点顺序必须是 {右下, 右上, 左下, 左上} 或类似的连续顺序

---

### 差异4: 顶点坐标更新方式不同

**v1.0.8 (FFmpegView.cc:1169-1171):**
```cpp
// 每次绘制前，更新整个交错数组（位置+纹理坐标一起）
glBindBuffer(GL_ARRAY_BUFFER, VBO);
glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
// vertices 包含 {x,y,z,tex_x,tex_y} 完整数据
```

**当前版本 (我的修改):**
```cpp
// 只更新位置VBO，纹理坐标VBO不更新
glBindBuffer(GL_ARRAY_BUFFER, m_vboPosition);
glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
// 纹理坐标VBO在初始化后从未更新！
```

**关键问题**: 纹理坐标可能需要与顶点位置同步更新！

---

### 差异5: 纹理坐标定义

**v1.0.8:**
```cpp
// 纹理坐标嵌入在顶点数据中，随顶点一起更新
vertices = {
    1.0,  p, 0.0,     1.0, 0.0,  // 纹理(1,0) - 右上
    1.0, -p, 0.0,     1.0, 1.0,  // 纹理(1,1) - 右下
   -1.0, -p, 0.0,     0.0, 1.0,  // 纹理(0,1) - 左下
   -1.0,  p, 0.0,     0.0, 0.0,  // 纹理(0,0) - 左上
};
```

**当前版本 (OpenGLVertices.hpp):**
```cpp
// 纹理坐标单独定义
constexpr float texCoordOSD[8] = {
    0.0f, 1.0f,  // 左下
    1.0f, 1.0f,  // 右下
    0.0f, 0.0f,  // 左上
    1.0f, 0.0f,  // 右上
};
```

**关键差异**: 顶点顺序和纹理坐标映射可能不匹配！

---

## 顶点顺序与纹理坐标映射分析

### GL_TRIANGLE_STRIP 顶点顺序要求

对于 `glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)`，顶点顺序应该是：
```
顶点0 ---- 顶点1
  |          |
  |          |
顶点2 ---- 顶点3
```

绘制顺序：三角形1 = {0,1,2}, 三角形2 = {1,2,3}

### 正确的顶点-纹理映射

| 顶点索引 | 位置坐标 | 纹理坐标 |
|---------|---------|---------|
| 0 | (-1, -1) 左下 | (0, 1) 纹理左下 |
| 1 | ( 1, -1) 右下 | (1, 1) 纹理右下 |
| 2 | (-1,  1) 左上 | (0, 0) 纹理左上 |
| 3 | ( 1,  1) 右上 | (1, 0) 纹理右上 |

### 当前版本的顶点数据

**位置 (verticesYCbCr[0]):**
```
顶点0: (-1.0, -1.0) 左下
顶点1: ( 1.0, -1.0) 右下
顶点2: (-1.0,  1.0) 左上
顶点3: ( 1.0,  1.0) 右上
```

**纹理坐标 (texCoordYCbCr from OpenGLCommon.hpp):**
```cpp
float texCoordYCbCr[8] = {...};  // 需要确认实际值
```

---

## 可能的问题点

### 1. 纹理坐标未与顶点位置同步

当`updateDynamicVertices()`修改顶点位置后，纹理坐标没有对应更新。例如：

```cpp
// 当视频更宽时（上下留黑）
vertices = {
    1.0,  p,  // 顶点位置改变
    1.0, -p,
   -1.0, -p,
   -1.0,  p,
};
// 但纹理坐标仍然是 {0,1}, {1,1}, {0,0}, {1,0}
// 这可能导致纹理采样位置错误
```

### 2. YUV纹理采样问题

Fragment shader中的YUV采样：

```glsl
float y = stereo_display(textureY, TexCoord, ...).r;
vec2 uv = stereo_display(textureUV, TexCoord, ...).rg;  // NV12
```

如果纹理坐标不正确，Y/U/V分量会采样到错误的位置。

### 3. OpenGL坐标系差异

OpenGL纹理坐标原点在左下角(0,0)，而图像通常以左上角为原点。这可能导致图像上下翻转。

---

## 修复方案

### 方案A: 完全匹配v1.0.8格式（推荐）

1. **修改vertex.glsl**：
```glsl
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

void main() {
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
```

2. **修改顶点数据为交错数组**：
```cpp
// 每个顶点5个float: {x, y, z, tex_x, tex_y}
float vertices[20] = {
     1.0,  p, 0.0,     1.0, 0.0,  // 右上
     1.0, -p, 0.0,     1.0, 1.0,  // 右下
    -1.0, -p, 0.0,     0.0, 1.0,  // 左下
    -1.0,  p, 0.0,     0.0, 0.0,  // 左上
};

// 设置顶点属性
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
```

3. **使用EBO绘制**：
```cpp
unsigned int indices[6] = {0, 1, 3, 1, 2, 3};
glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
```

### 方案B: 修复当前格式的顶点-纹理映射

1. **确保纹理坐标与顶点位置对应**：
```cpp
// 在 updateDynamicVertices() 中同时更新纹理坐标
float vertices[8] = {...};      // 位置
float texCoords[8] = {          // 纹理坐标（确保顺序正确）
    0.0f, 1.0f,  // 对应顶点0
    1.0f, 1.0f,  // 对应顶点1
    0.0f, 0.0f,  // 对应顶点2
    1.0f, 0.0f,  // 对应顶点3
};
```

2. **更新两个VBO**：
```cpp
glBindBuffer(GL_ARRAY_BUFFER, m_vboPosition);
glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
glBindBuffer(GL_ARRAY_BUFFER, m_vboTexCoord);
glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(texCoords), texCoords);
```

---

---

## 关键发现：纹理坐标初始化问题

### OpenGLCommon.cpp 中的纹理坐标初始化

```cpp
// OpenGLCommon.cpp:105-106
texCoordYCbCr[0] = texCoordYCbCr[4] = texCoordYCbCr[5] = texCoordYCbCr[7] = 0.0f;
texCoordYCbCr[1] = texCoordYCbCr[3] = 1.0f;
// 注意：texCoordYCbCr[2] 和 texCoordYCbCr[6] 没有被初始化！
```

**实际初始化后的值：**
| 索引 | 值 | 顶点 | 纹理坐标 |
|-----|-----|-----|---------|
| 0 | 0.0f | 顶点0 (左下) | u=0.0 |
| 1 | 1.0f | 顶点0 (左下) | v=1.0 |
| 2 | **未初始化！** | 顶点1 (右下) | u=? |
| 3 | 1.0f | 顶点1 (右下) | v=1.0 |
| 4 | 0.0f | 顶点2 (左上) | u=0.0 |
| 5 | 0.0f | 顶点2 (左上) | v=0.0 |
| 6 | **未初始化！** | 顶点3 (右上) | u=? |
| 7 | 0.0f | 顶点3 (右上) | v=0.0 |

**后续更新（StereoOpenGLCommon.cpp:461）：**
```cpp
texCoordYCbCr[2] = texCoordYCbCr[6] = (videoFrame.linesize(0) / bytesMultiplier == widths[0])
    ? 1.0f
    : (widths[0] / (videoFrame.linesize(0) / bytesMultiplier + 1.0f));
```

这只在 `doReset=true` 时执行，如果首帧渲染时 `texCoordYCbCr[2]` 和 `[6]` 是随机值，会导致严重的纹理采样错误！

---

## 顶点顺序对比

### 当前版本顶点顺序 (GL_TRIANGLE_STRIP)

```
顶点2 (-1, +1) ---- 顶点3 (+1, +1)
     |                    |
     |                    |
顶点0 (-1, -1) ---- 顶点1 (+1, -1)
```

绘制顺序：三角形{0,1,2}，三角形{1,2,3}

### v1.0.8 顶点顺序 (GL_TRIANGLES + EBO)

```
顶点3 (-1, +p) ---- 顶点0 (+1, +p)
     |                    |
     |                    |
顶点2 (-1, -p) ---- 顶点1 (+1, -p)
```

EBO索引：{0,1,3, 1,2,3}，绘制两个三角形

### 纹理坐标期望值

对于正常视频渲染（无翻转），纹理坐标应该是：
- 左下 (顶点0): (0.0, 1.0) - OpenGL纹理原点在左下
- 右下 (顶点1): (1.0, 1.0)
- 左上 (顶点2): (0.0, 0.0)
- 右上 (顶点3): (1.0, 0.0)

**问题**：当前 `texCoordYCbCr[2]` 和 `[6]` 应该始终是 `1.0f`，但初始化时未设置！

---

## 调试建议

1. **保存渲染前后的帧**：
   - 保存`videoFrame`原始数据（Y/U/V平面）
   - 保存渲染后的framebuffer
   - 对比两者差异

2. **验证纹理上传**：
   - 检查`glTexImage2D`/`glTexSubImage2D`参数
   - 确认纹理尺寸和linesize正确

3. **验证顶点属性**：
   - 打印顶点位置和纹理坐标的实际值
   - 使用RenderDoc或类似工具捕获帧

4. **简化测试**：
   - 先测试2D模式（iStereoFlag=0）
   - 确认基本YUV->RGB转换正确
   - 再测试3D模式

---

## 相关文件

| 文件 | 说明 |
|-----|------|
| `StereoOpenGLCommon.cpp` | 主要渲染逻辑 |
| `StereoOpenGLCommon.hpp` | 类定义和成员变量 |
| `OpenGLCommon.hpp` | 父类，包含纹理坐标定义 |
| `OpenGLVertices.hpp` | 预定义顶点数据 |
| `Shader/vertex.glsl` | 顶点着色器 |
| `Shader/fragment.glsl` | 片段着色器 |
| `v1.0.8-srcs/WZMediaPlay/FFmpegView.cc` | 参考实现 |
| `v1.0.8-srcs/WZMediaPlay/Shader/vertex.glsl` | 参考shader |

---

*文档更新时间: 2026-03-11*
*分析者: Claude Code*