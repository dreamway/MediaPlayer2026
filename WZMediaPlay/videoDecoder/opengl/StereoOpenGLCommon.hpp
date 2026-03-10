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

    // VBO (Vertex Buffer Object) - OpenGL Core Profile 必需，用于存储顶点数据
    quint32 m_vboPosition = 0;  // 位置 VBO
    quint32 m_vboTexCoord = 0;  // 纹理坐标 VBO
    bool m_vboInitialized = false;  // VBO 是否已初始化
};
