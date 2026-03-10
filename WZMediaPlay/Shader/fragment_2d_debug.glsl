#version 150 core
// 简单的 2D YUV 渲染 shader（用于调试）
// 不进行任何 stereo 处理，只做基本的 YUV 到 RGB 转换

out vec4 FragColor;
in vec2 TexCoord;

uniform sampler2D textureY;
uniform sampler2D textureU;
uniform sampler2D textureV;

// 视频格式：0=RGB, 1=NV12, 2=YUV420P
uniform int iVideoFormat;

void main()
{
    vec3 yuv, rgb;

    // 获取 Y 值
    float y = texture(textureY, TexCoord).r;

    // 根据 iVideoFormat 区分 NV12 和 YUV420P 格式
    float u, v;
    if (iVideoFormat == 1) {
        // NV12 格式：UV 交织在一个纹理中
        vec2 uv = texture(textureU, TexCoord).rg;  // 注意：使用 textureU 作为 UV 纹理
        u = uv.r;
        v = uv.g;
    } else {
        // YUV420P 格式：U 和 V 是独立的纹理
        u = texture(textureU, TexCoord).r;
        v = texture(textureV, TexCoord).r;
    }

    // YUV 到 RGB 转换（BT.601 标准，Full Range）
    // 假设输入是 Full Range（0-255）
    yuv.x = y;
    yuv.y = u - 0.5;
    yuv.z = v - 0.5;

    // BT.601 YUV 到 RGB 转换矩阵
    rgb.x = yuv.x + 1.402 * yuv.z;
    rgb.y = yuv.x - 0.344136 * yuv.y - 0.714136 * yuv.z;
    rgb.z = yuv.x + 1.772 * yuv.y;

    // 限制 RGB 值在 0-1 范围内
    rgb = clamp(rgb, 0.0, 1.0);

    FragColor = vec4(rgb, 1.0);
}