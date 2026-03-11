/*
    QMPlay2 is a video and audio player.
    Copyright (C) 2010-2025  Błażej Szczygieł

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "OpenGLCommon.hpp"
#include "../../GlobalDef.h"
#include <QOpenGLVertexArrayObject>

/**
 * StereoOpenGLCommon: 支持 3D 立体渲染的 OpenGL Common
 * 
 * 继承自 OpenGLCommon，添加 3D Shader 支持
 * - 2D 模式：复用 OpenGLCommon 的 2D 渲染
 * - 3D 模式：使用 3D Shader 进行立体渲染
 */
class StereoOpenGLCommon : public OpenGLCommon
{
    Q_DECLARE_TR_FUNCTIONS(StereoOpenGLCommon)

public:
    StereoOpenGLCommon();
    ~StereoOpenGLCommon();

    // 重写 paintGL，根据 StereoFormat 选择 2D 或 3D 渲染
    void paintGL() override;

    // 3D 参数设置方法（由 StereoWriter 调用）
    void setStereoFormat(StereoFormat format);
    void setStereoInputFormat(StereoInputFormat inputFormat);
    void setStereoOutputFormat(StereoOutputFormat outputFormat);
    void setParallaxShift(int shift);
    void setStereoEnableRegion(bool enable, float topLeftX, float topLeftY, float bottomRightX, float bottomRightY);
    
    // 切换使用默认 2D Shader（用于调试对比）
    void setUseDefault2DShader(bool use) { m_useDefault2DShader = use; }
    bool isUsingDefault2DShader() const { return m_useDefault2DShader; }

protected:
    // 初始化 3D Shader（在 initializeGL 中调用）
    void initializeStereoShader();

    // 设置 3D Shader 的 uniform 参数
    void setStereoShaderUniforms();

    // 使用 3D shader 进行渲染
    void paintGLStereo();

    // 计算动态顶点坐标（参考v1.0.8 FFmpegView::programDraw）
    // 根据视频宽高比动态调整顶点，保持正确的显示比例
    void updateDynamicVertices(int frameWidth, int frameHeight);

    // 设置全屏Plus模式（拉伸显示）
    void setFullscreenPlusStretch(bool stretch) { m_fullscreenPlusStretch = stretch; }

    // 切换视差调节时的裁剪效果
    void toggleStripParallaxSideView() { m_enableStripParallaxSideView = !m_enableStripParallaxSideView; }
    
    // 切换使用默认 2D Shader（用于调试对比，继承自 OpenGLCommon）
    // void setUseDefault2DShader(bool use);  // 继承自 OpenGLCommon

private:
    // 3D 渲染参数
    StereoFormat m_stereoFormat = STEREO_FORMAT_NORMAL_2D;
    StereoInputFormat m_stereoInputFormat = STEREO_INPUT_FORMAT_LR;
    StereoOutputFormat m_stereoOutputFormat = STEREO_OUTPUT_FORMAT_HORIZONTAL;
    int m_parallaxShift = 0;
    bool m_enableStereoRegion = false;
    float m_stereoRegion[4] = {0.0f, 0.0f, 1.0f, 1.0f};  // topLeftX, topLeftY, bottomRightX, bottomRightY

    // 3D Shader 程序（用于 3D 渲染）
    std::unique_ptr<QOpenGLShaderProgram> shaderProgramStereo;
    bool m_stereoShaderInitialized = false;
    
    // 默认 2D Shader 程序（用于对比测试，可选）
    std::unique_ptr<QOpenGLShaderProgram> shaderProgramDefault2D;
    bool m_useDefault2DShader = false;  // 是否使用默认 2D Shader（用于调试）
    
    // 异步图片保存器（用于保存调试图片，避免影响渲染性能）
    class DebugImageSaver* debugImageSaver_;

    // VAO (Vertex Array Object) - OpenGL Core Profile 必需
    QOpenGLVertexArrayObject m_vao;

    // VBO (Vertex Buffer Object) - 使用交错数组格式，参考v1.0.8
    // 每个顶点5个float: {x, y, z, tex_x, tex_y}
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

    bool m_vboInitialized = false;  // VBO 是否已初始化

    // === 动态顶点计算相关成员（参考v1.0.8 FFmpegView实现） ===
    // 视口尺寸
    int m_viewWidth = 300;
    int m_viewHeight = 200;

    // 纹理尺寸
    int m_texWidth = -1;
    int m_texHeight = -1;

    // 宽高比
    float m_ratio = 1.0f;

    // 是否启用视差调节时的裁剪效果
    bool m_enableStripParallaxSideView = true;

    // 全屏Plus模式（拉伸显示）
    bool m_fullscreenPlusStretch = false;
};
