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

//#include "OpenGLInstance.hpp"

#include "../Frame.h"
#include "../VideoAdjustment.hpp"
#include "../VideoOutputCommon.hpp"

#include <QOpenGLShaderProgram>
#if !defined(QT_OPENGL_ES_2) && !defined(QT_FEATURE_opengles2)
#   include <QOpenGLFunctions_1_5>
#endif
#include <QOpenGLExtraFunctions>

#include <QCoreApplication>
#include <QImage>
#include <QTimer>

//class OpenGLHWInterop;

class OpenGLCommon : public VideoOutputCommon, public QOpenGLExtraFunctions
{
    Q_DECLARE_TR_FUNCTIONS(OpenGLCommon)

public:
    OpenGLCommon();
    virtual ~OpenGLCommon();

    virtual void deleteMe();

    virtual bool makeContextCurrent() = 0;
    virtual void doneContextCurrent() = 0;

    inline double &zoomRef()
    {
        return m_zoom;
    }
    inline double &aRatioRef()
    {
        return m_aRatio;;
    }

    void initialize(const std::shared_ptr<void> &hwInterop = nullptr);  // 简化：参数类型改为 void*，实际不使用（保留用于接口兼容性）

    virtual void setVSync(bool enable) = 0;
    virtual void updateGL(bool requestDelayed) = 0;


    void setWindowsBypassCompositor(bool bypassCompositor);

    void newSize(bool canUpdate);
    void clearImg();

    
protected:
    void setTextureParameters(GLenum target, quint32 texture, GLint param);

    void initializeGL();
    virtual void paintGL();

    void contextAboutToBeDestroyed();
    
public:
    // 切换使用默认 2D Shader（用于调试对比）
    void setUseDefault2DShader(bool use) { m_useDefault2DShader = use; }
    bool isUsingDefault2DShader() const { return m_useDefault2DShader; }

private:
    // 初始化默认 2D Shader（Video.vert + VideoYCbCr.frag）
    void initializeDefault2DShader();


protected:
    bool vSync;
    void dispatchEvent(QEvent *e, QObject *p) override;
protected:
    QByteArray readShader(const QString &fileName, bool pure = false);
private:
    inline bool isRotate90() const;

public:
    // 移除对 OpenGLInstance 和 OpenGLHWInterop 的依赖
    // 简化设计：只使用 hardware_decoder 输出 NV12 格式，直接渲染
    // std::shared_ptr<OpenGLInstance> m_glInstance;  // 已移除
    // std::shared_ptr<OpenGLHWInterop> m_hwInterop;  // 已移除
    QStringList videoAdjustmentKeys;
    Frame videoFrame;

    AVColorPrimaries m_colorPrimaries = AVCOL_PRI_UNSPECIFIED;
    AVColorTransferCharacteristic m_colorTrc = AVCOL_TRC_UNSPECIFIED;
    AVColorSpace m_colorSpace = AVCOL_SPC_UNSPECIFIED;
    float m_maxLuminance = 1000.0f;
    float m_bitsMultiplier = 1.0f;
    int m_depth = 8;
    bool m_limited = false;

    std::unique_ptr<QOpenGLShaderProgram> shaderProgramVideo, shaderProgramOSD;
    
    // 默认 2D Shader 程序（用于对比测试，可选）
    std::unique_ptr<QOpenGLShaderProgram> shaderProgramDefault2D;
    bool m_useDefault2DShader = false;  // 是否使用默认 2D Shader（用于调试，默认 false 使用自定义 shader）

    qint32 texCoordYCbCrLoc, positionYCbCrLoc, texCoordOSDLoc, positionOSDLoc;
    VideoAdjustment videoAdjustment;
    float texCoordYCbCr[8];
    quint32 textures[4];
    QSize m_textureSize;
    qint32 numPlanes;
    quint32 target;

    bool m_canUse16bitTexture = false;

    quint32 pbo[4];
    bool hasPbo;

    bool isPaused, isOK, hasImage, doReset, setMatrix, correctLinesize, m_gl3;
    int outW, outH, verticesIdx;

    // 保存的帧高度（在清除帧之前保存，用于颜色空间猜测）
    int m_lastFrameHeight = 0;

    // OSD 支持已移除，只保留核心渲染功能

    QTimer updateTimer;
};
