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

#include "OpenGLCommon.hpp"
// #include "OpenGLHWInterop.hpp"  // 已移除，不再使用
#include "OpenGLVertices.hpp"

#include "../../GlobalDef.h"
#include "../Frame.h"
// #include "../GPUInstance.hpp"  // 已移除，不再使用
#include "Functions.hpp"


#include <chrono>
#include <libswscale/swscale.h>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QImage>
#include <QLibrary>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLShader>
#include <QPainter>
#include <QResizeEvent>
#include <QResource>
#include <QString>
#include <QTextStream>
#include <QWidget>

#ifdef Q_OS_WIN
#include <QOperatingSystemVersion>
#endif

/* OpenGL|ES 2.0 doesn't have those definitions */
#ifndef GL_MAP_WRITE_BIT
#define GL_MAP_WRITE_BIT 0x0002
#endif
#ifndef GL_MAP_INVALIDATE_BUFFER_BIT
#define GL_MAP_INVALIDATE_BUFFER_BIT 0x0008
#endif
#ifndef GL_WRITE_ONLY
#define GL_WRITE_ONLY 0x88B9
#endif
#ifndef GL_PIXEL_UNPACK_BUFFER
#define GL_PIXEL_UNPACK_BUFFER 0x88EC
#endif
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif
#ifndef GL_R16
#define GL_R16 GL_R16_EXT
#endif
#ifndef GL_RED
#define GL_RED GL_RED_EXT
#endif

#define INSPECT

OpenGLCommon::OpenGLCommon()
    : VideoOutputCommon(false)
    , vSync(true)
    // m_glInstance 已移除，使用默认值
    , shaderProgramVideo(nullptr)
    , shaderProgramOSD(nullptr)
    , // OSD shader 保留但不使用
    texCoordYCbCrLoc(-1)
    , positionYCbCrLoc(-1)
    , texCoordOSDLoc(-1)
    , positionOSDLoc(-1)
    , numPlanes(0)
    , target(0)
    , m_canUse16bitTexture(false)  // 默认值，简化设计
    , hasPbo(false)  // 默认值，简化设计
    , isPaused(false)
    , isOK(false)
    , hasImage(false)
    , doReset(true)
    , setMatrix(true)
    , correctLinesize(false)
    , m_gl3(true)
    , outW(-1)
    , outH(-1)
    , verticesIdx(0)
{
    videoAdjustment.unset();

    /* Initialize texCoordYCbCr array */
    texCoordYCbCr[0] = texCoordYCbCr[4] = texCoordYCbCr[5] = texCoordYCbCr[7] = 0.0f;
    texCoordYCbCr[1] = texCoordYCbCr[3] = 1.0f;

#ifndef Q_OS_MACOS
    // m_gl3 默认值已设置为 true
#endif

    m_matrixChangeFn = [this] {
        setMatrix = true;
        updateGL(true);
    };
}
OpenGLCommon::~OpenGLCommon()
{
    contextAboutToBeDestroyed();
}

void OpenGLCommon::deleteMe()
{
    delete this;
}

void OpenGLCommon::initialize(const std::shared_ptr<void> &hwInterop)
{
    // 简化设计：不再使用 OpenGLHWInterop
    // hardware_decoder 直接输出 NV12 格式的软件帧，直接渲染即可
    if (isOK)
        return;

    isOK = true;

    // 默认使用 3 个平面（YUV420P）或 2 个平面（NV12）
    // hardware_decoder 输出 NV12，所以使用 2 个平面
    numPlanes = 2;  // NV12 格式
    target = GL_TEXTURE_2D;

    const bool windowContext = makeContextCurrent();

    if (windowContext)
        contextAboutToBeDestroyed();

    videoAdjustmentKeys.clear();

    // 不再使用硬件互操作，直接渲染软件帧

    if (windowContext) {
        initializeGL();
        doneContextCurrent();
    }
}

// #ifdef Q_OS_WIN
// void OpenGLCommon::setWindowsBypassCompositor(bool bypassCompositor)
// {
//     if (!bypassCompositor && QOperatingSystemVersion::current() <= QOperatingSystemVersion::Windows7) // Windows 7 and Vista can disable DWM composition, so check it
//     {
//         using DwmIsCompositionEnabledProc = HRESULT (WINAPI *)(BOOL *pfEnabled);
//         if (auto DwmIsCompositionEnabled = (DwmIsCompositionEnabledProc)GetProcAddress(GetModuleHandleA("dwmapi.dll"), "DwmIsCompositionEnabled"))
//         {
//             BOOL enabled = false;
//             if (DwmIsCompositionEnabled(&enabled) == S_OK && !enabled)
//                 bypassCompositor = true; // Don't try to avoid compositor bypass if compositor is disabled
//         }
//     }
//     widget()->setProperty("bypassCompositor", bypassCompositor ? 1 : -1);
// }
// #endif

void OpenGLCommon::newSize(bool canUpdate)
{
    updateSizes(isRotate90());
    doReset = true;
    if (canUpdate) {
        if (isPaused)
            updateGL(false);
        else if (!updateTimer.isActive())
            updateTimer.start(40);
    }
}
void OpenGLCommon::clearImg()
{
    hasImage = false;
    videoFrame.clear();
}

void OpenGLCommon::setTextureParameters(GLenum target, quint32 texture, GLint param)
{
    glBindTexture(target, texture);
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, param);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, param);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(target, 0);
}

void OpenGLCommon::initializeGL()
{
    initializeOpenGLFunctions();

#if !defined(QT_OPENGL_ES_2) && !defined(QT_FEATURE_opengles2)
    // 简化设计：不再检查 hasMapBufferRange，直接使用默认行为
    if (hasPbo) {
        m_gl15.initializeOpenGLFunctions();
    }
#endif

    shaderProgramVideo.reset(new QOpenGLShaderProgram);
    // OSD shader 已禁用
    // shaderProgramOSD.reset(new QOpenGLShaderProgram);

    /* 使用自定义 shader（从 Shader/ 目录加载，优先外部，否则使用内部资源） */
    QString vertexSource, fragmentSource;

    // 先尝试加载外部 shader
    QFile vertexFile("./Shader/vertex.glsl");
    if (vertexFile.exists() && vertexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream vertexIn(&vertexFile);
        vertexSource = vertexIn.readAll();
        vertexFile.close();
        if (logger)
            logger->info("OpenGLCommon: Loaded external vertex shader");
    }

    QFile fragmentFile("./Shader/fragment.glsl");
    if (fragmentFile.exists() && fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream fragIn(&fragmentFile);
        fragmentSource = fragIn.readAll();
        fragmentFile.close();
        if (logger)
            logger->info("OpenGLCommon: Loaded external fragment shader");
    }

    // 如果外部 shader 不存在，尝试加载内部 shader
    if (vertexSource.isEmpty()) {
        QFile vertexFileInternal(":/MainWindow/Shader/vertex.glsl");
        if (vertexFileInternal.exists() && vertexFileInternal.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream vertexIn(&vertexFileInternal);
            vertexSource = vertexIn.readAll();
            vertexFileInternal.close();
            if (logger)
                logger->info("OpenGLCommon: Loaded internal vertex shader");
        }
    }

    if (fragmentSource.isEmpty()) {
        QFile fragmentFileInternal(":/MainWindow/Shader/fragment.glsl");
        if (fragmentFileInternal.exists() && fragmentFileInternal.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream fragIn(&fragmentFileInternal);
            fragmentSource = fragIn.readAll();
            fragmentFileInternal.close();
            if (logger)
                logger->info("OpenGLCommon: Loaded internal fragment shader");
        }
    }

    // 检查 shader 是否加载成功
    if (vertexSource.isEmpty() || fragmentSource.isEmpty()) {
        if (logger)
            logger->error(
                "OpenGLCommon: Failed to load shader sources (vertex: {}, fragment: {})",
                vertexSource.isEmpty() ? "empty" : "ok",
                fragmentSource.isEmpty() ? "empty" : "ok");
        isOK = false;
        return;
    }

    // 转换 QString 到 QByteArray
    QByteArray vertexShader = vertexSource.toUtf8();
    QByteArray videoFrag = fragmentSource.toUtf8();

    // fragment.glsl 已经包含了所有必要的逻辑，不需要动态插入宏定义
    // 格式信息（NV12/YUV420P/RGB）通过 uniform sampler（textureY/textureU/textureV/textureUV）传递
    // OpenGL 版本和纹理类型信息由 shader 本身处理，不需要宏定义

    // 编译 shader
    if (!shaderProgramVideo->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader)) {
        if (logger)
            logger->error("OpenGLCommon: Vertex shader compile failed: {}", shaderProgramVideo->log().toStdString());
        isOK = false;
        return;
    }

    if (!shaderProgramVideo->addShaderFromSourceCode(QOpenGLShader::Fragment, videoFrag)) {
        QString errorLog = shaderProgramVideo->log();
        if (logger) {
            logger->error("OpenGLCommon: Fragment shader compile failed: {}", errorLog.toStdString());
        }
        isOK = false;
        return;
    }

    if (!shaderProgramVideo->link()) {
        if (logger)
            logger->error("OpenGLCommon: Shader link failed: {}", shaderProgramVideo->log().toStdString());
        isOK = false;
        return;
    }

    if (shaderProgramVideo->bind()) {
        // 获取 attribute locations（自定义 shader 使用 aPos (vec2) 和 aTexCoord）
        texCoordYCbCrLoc = shaderProgramVideo->attributeLocation("aTexCoord");
        positionYCbCrLoc = shaderProgramVideo->attributeLocation("aPos"); // 自定义 shader 使用 aPos

        if (texCoordYCbCrLoc < 0 || positionYCbCrLoc < 0) {
            if (logger)
                logger->warn("OpenGLCommon: Attribute locations - aTexCoord: {}, aPos: {}", texCoordYCbCrLoc, positionYCbCrLoc);
        }

        // 设置纹理 uniform（fragment.glsl 使用 textureY, textureU, textureV, textureUV）
        shaderProgramVideo->setUniformValue("textureY", 0);
        if (numPlanes == 2) {
            shaderProgramVideo->setUniformValue("textureUV", 1);
            shaderProgramVideo->setUniformValue("textureU", -1);
            shaderProgramVideo->setUniformValue("textureV", -1);
        } else if (numPlanes == 3) {
            shaderProgramVideo->setUniformValue("textureU", 1);
            shaderProgramVideo->setUniformValue("textureV", 2);
            shaderProgramVideo->setUniformValue("textureUV", -1);
        } else {
            // 单平面格式（RGB），只使用 textureY
            shaderProgramVideo->setUniformValue("textureU", -1);
            shaderProgramVideo->setUniformValue("textureV", -1);
            shaderProgramVideo->setUniformValue("textureUV", -1);
        }

        shaderProgramVideo->release();
    } else {
        if (logger)
            logger->error("OpenGLCommon: Shader compile/link error");
        isOK = false;
        return;
    }

    /* Set OpenGL parameters */
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_DITHER);

    /* Prepare textures */
    // 简化设计：不再使用硬件互操作，总是生成纹理
    const int texturesToGen = numPlanes;
    glGenTextures(texturesToGen + 1, textures);
    for (int i = 0; i < texturesToGen + 1; ++i) {
        const quint32 tmpTarget = (i == 0) ? GL_TEXTURE_2D : target;
        setTextureParameters(tmpTarget, textures[i], (i == 0) ? GL_NEAREST : GL_LINEAR);
    }

    if (hasPbo) {
        glGenBuffers(1 + texturesToGen, pbo);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    }

    setVSync(vSync);

    doReset = true;
}

void OpenGLCommon::paintGL()
{
    // 使用 try-catch 保护整个 paintGL，避免崩溃
    try {
        // 简化设计：不再检查硬件帧，直接使用软件帧
        const bool frameIsEmpty = videoFrame.isEmpty();

        if (updateTimer.isActive())
            updateTimer.stop();

        // 如果当前帧为空且从未渲染过，清屏为黑再返回（修复 BUG-022：播放结束后花屏/绿品红）
        if (frameIsEmpty && !hasImage) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            return;
        }

        // 如果当前帧为空但之前有渲染过，不 return：跳过纹理上传，但继续执行后面的 glDrawArrays，
        // 用已有纹理再画一帧，避免无新帧时出现黑屏闪烁（BUG 6）

        bool resetDone = false;

        if (!frameIsEmpty) {
            const GLsizei widths[3] = {
                videoFrame.width(0),
                videoFrame.width(1),
                videoFrame.width(2),
            };
            const GLsizei heights[3] = {
                videoFrame.height(0),
                videoFrame.height(1),
                videoFrame.height(2),
            };
            const int bytesMultiplier = (m_depth + 7) / 8;
            const GLenum dataType = (bytesMultiplier == 1) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;
            const GLint internalFmt = (bytesMultiplier == 1) ? GL_LUMINANCE : GL_R16;
            const GLenum fmt = (bytesMultiplier == 1) ? GL_LUMINANCE : GL_RED;

            if (doReset) {
                // 简化设计：不再使用硬件互操作，直接使用软件帧路径
                {
                    /* Check linesize */
                    const qint32 halfLinesize = (videoFrame.linesize(0) >> videoFrame.chromaShiftW());
                    correctLinesize
                        = ((halfLinesize == videoFrame.linesize(1) && videoFrame.linesize(1) == videoFrame.linesize(2))
                           && (!m_sphericalView ? (videoFrame.linesize(1) == halfLinesize) : (videoFrame.linesize(0) == widths[0])));

                    /* Prepare textures */
                    for (qint32 p = 0; p < 3; ++p) {
                        const GLsizei w = correctLinesize ? videoFrame.linesize(p) / bytesMultiplier : widths[p];
                        const GLsizei h = heights[p];
                        if (p == 0)
                            m_textureSize = QSize(w, h);
                        if (hasPbo) {
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[p + 1]);
                            glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * bytesMultiplier, nullptr, GL_DYNAMIC_DRAW);
                        }
                        glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
                        glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, dataType, nullptr);
                    }

                    /* Prepare texture coordinates */
                    texCoordYCbCr[2] = texCoordYCbCr[6] = (videoFrame.linesize(0) / bytesMultiplier == widths[0])
                                                              ? 1.0f
                                                              : (widths[0] / (videoFrame.linesize(0) / bytesMultiplier + 1.0f));
                }
                resetDone = true;
                // 注释掉 hasImage = false; 以避免 doReset 导致黑画面
                // hasImage = false;
            }

            // 简化设计：不再使用硬件互操作，直接使用软件帧渲染路径
            {
                /* Load textures */
                // 在加载纹理前，验证帧数据完整性（避免在转换过程中数据被修改导致花屏）
                bool frameDataValid = true;
                for (qint32 p = 0; p < 3 && p < numPlanes; ++p) {
                    const quint8 *data = videoFrame.constData(p);
                    if (!data) {
                        if (logger) {
                            logger->warn("OpenGLCommon::paintGL: Frame plane {} data is null, skipping texture upload", p);
                        }
                        frameDataValid = false;
                        break;
                    }
                }

                if (!frameDataValid) {
                    // 帧数据无效，保留上一帧（避免闪烁）
                    if (logger) {
                        logger->warn("OpenGLCommon::paintGL: Frame data invalid, keeping previous frame");
                    }
                    return; // 不更新纹理，保留上一帧
                }

                for (qint32 p = 0; p < 3; ++p) {
                    const quint8 *data = videoFrame.constData(p);
                    if (!data) {
                        // 如果某个平面数据为空，跳过该平面（避免崩溃）
                        if (logger && p < numPlanes) {
                            logger->warn("OpenGLCommon::paintGL: Frame plane {} data is null, skipping", p);
                        }
                        continue;
                    }

                    const GLsizei w = correctLinesize ? videoFrame.linesize(p) / bytesMultiplier : widths[p];
                    const GLsizei h = heights[p];

                    // 验证尺寸有效性
                    if (w <= 0 || h <= 0) {
                        if (logger) {
                            logger->warn("OpenGLCommon::paintGL: Invalid texture dimensions (plane {}: {}x{}), skipping", p, w, h);
                        }
                        continue;
                    }

                    if (hasPbo) {
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[p + 1]);
                        quint8 *dst = nullptr;
                        // 简化设计：假设支持 mapBufferRange（现代 OpenGL 都支持）
                        if (true)  // 总是使用 mapBufferRange
                            dst = (quint8 *)
                                glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, w * h * bytesMultiplier, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#if !defined(QT_OPENGL_ES_2) && !defined(QT_FEATURE_opengles2)
                        else
                            dst = (quint8 *) m_gl15.glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#endif
                        if (!dst)
                            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                        else {
                            try {
                                if (correctLinesize)
                                    memcpy(dst, data, w * h * bytesMultiplier);
                                else
                                    for (int y = 0; y < h; ++y) {
                                        memcpy(dst, data, w * bytesMultiplier);
                                        data += videoFrame.linesize(p);
                                        dst += w * bytesMultiplier;
                                    }
                            } catch (const std::exception &e) {
                                if (logger) {
                                    logger->error("OpenGLCommon::paintGL: Exception while copying frame data (plane {}): {}", p, e.what());
                                }
                                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                                continue; // 跳过该平面
                            } catch (...) {
                                if (logger) {
                                    logger->error("OpenGLCommon::paintGL: Unknown exception while copying frame data (plane {})", p);
                                }
                                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                                continue; // 跳过该平面
                            }
                            glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                            data = nullptr;
                        }
                    }
                    glActiveTexture(GL_TEXTURE0 + p);
                    glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
                    if (hasPbo || correctLinesize)
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, fmt, dataType, data);
                    else
                        for (int y = 0; y < h; ++y) {
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, 1, fmt, dataType, data);
                            data += videoFrame.linesize(p);
                        }
                }
                if (hasPbo)
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

                // 添加软件解码纹理绑定日志（每100帧记录一次）
                static int swTextureLogCounter = 0;
                swTextureLogCounter++;
                if (logger && swTextureLogCounter % 100 == 0) {
                    logger->debug(
                        "OpenGLCommon::paintGL: Software textures uploaded, numPlanes: {}, widths: [{}, {}, {}], heights: [{}, {}, {}]",
                        numPlanes,
                        widths[0],
                        widths[1],
                        widths[2],
                        heights[0],
                        heights[1],
                        heights[2]);
                }
            }

            // 清除帧数据以释放内存（避免队列满）
            // 简化设计：数据已经上传到纹理，可以清除 CPU 端的数据
            videoFrame.clear();
            hasImage = true;
        } else {
            // 如果 videoFrame 为空，但有 hasImage，保留上一帧继续渲染（避免闪烁）
            // 如果 hasImage 也为 false，说明从未渲染过，直接返回
            if (!hasImage) {
                return; // 从未渲染过，直接返回
            }
            // 否则继续使用上一帧的数据进行渲染（hasImage 保持为 true）

            // 调试：每100次 paintGL 调用记录一次日志
            static int paintGLCounter = 0;
            paintGLCounter++;

            if (paintGLCounter % 100 == 0) {
                if (logger)
                    logger->debug("OpenGLCommon::paintGL called {} times, videoFrame.isEmpty(): {}, hasImage: {}", paintGLCounter, videoFrame.isEmpty(), hasImage);
            }
        }

        // 自定义 shader 使用 aPos (vec2)，直接使用 2D 坐标
        shaderProgramVideo->setAttributeArray(positionYCbCrLoc, verticesYCbCr[verticesIdx], 2);
        shaderProgramVideo->setAttributeArray(texCoordYCbCrLoc, texCoordYCbCr, 2);

        shaderProgramVideo->enableAttributeArray(positionYCbCrLoc);
        shaderProgramVideo->enableAttributeArray(texCoordYCbCrLoc);

        shaderProgramVideo->bind();

        // 设置视频格式 uniform：0=RGB, 1=NV12, 2=YUV420P
        int videoFormat = 0; // 默认 RGB
        if (numPlanes == 2) {
            videoFormat = 1; // NV12
        } else if (numPlanes == 3) {
            videoFormat = 2; // YUV420P
        }
        shaderProgramVideo->setUniformValue("iVideoFormat", videoFormat);

        // 设置颜色空间转换 uniform（核心功能：只在 YUV 格式时设置）
        if (videoFormat != 0) {
            // YUV 到 RGB 转换矩阵
            const QMatrix3x3 mat = Functions::getYUVtoRGBmatrix(m_colorSpace);
            shaderProgramVideo->setUniformValue("uYUVtRGB", mat);
        }

        // 添加 Shader 使用日志（每100帧记录一次）
        static int shaderLogCounter = 0;
        shaderLogCounter++;
        if (logger && shaderLogCounter % 100 == 0) {
            logger->info(
                "OpenGLCommon::paintGL: Using custom shader (Shader/vertex.glsl + fragment.glsl), videoFormat: {}, numPlanes: {}, hasImage: {}, "
                "videoFrame.isEmpty(): {}",
                videoFormat,
                numPlanes,
                hasImage,
                videoFrame.isEmpty());
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        shaderProgramVideo->release();

        shaderProgramVideo->disableAttributeArray(texCoordYCbCrLoc);
        shaderProgramVideo->disableAttributeArray(positionYCbCrLoc);

        glBindTexture(GL_TEXTURE_2D, 0);
    } catch (const std::exception &e) {
        if (logger) {
            logger->error("OpenGLCommon::paintGL: Exception: {}", e.what());
        }
        // 异常时不清除 hasImage，保留上一帧（避免闪烁）
    } catch (...) {
        if (logger) {
            logger->error("OpenGLCommon::paintGL: Unknown exception");
        }
        // 异常时不清除 hasImage，保留上一帧（避免闪烁）
    }
}

void OpenGLCommon::contextAboutToBeDestroyed()
{
    // 简化设计：不再使用硬件互操作
    const int texturesToDel = numPlanes;
    if (hasPbo)
        glDeleteBuffers(1 + texturesToDel, pbo);
    glDeleteTextures(texturesToDel + 1, textures);
}

void OpenGLCommon::dispatchEvent(QEvent *e, QObject *p)
{
    if (e->type() == QEvent::Resize)
        newSize(false);
    VideoOutputCommon::dispatchEvent(e, p);
}

inline bool OpenGLCommon::isRotate90() const
{
    return verticesIdx >= 4 && !m_sphericalView;
}

QByteArray OpenGLCommon::readShader(const QString &fileName, bool pure)
{
    QResource res(fileName);
    QByteArray shader;
    if (!pure) {
        // 简化设计：假设不是 GLES（桌面 OpenGL）
        if (false)  // 不是 GLES
            shader = "precision highp float;\n";
        shader.append("#line 1\n");
    }
    if (!res.isValid()) {
        if (logger)
            logger->error("OpenGLCommon: Shader resource not found: {}", fileName.toStdString());
        return shader; // 返回空 shader，会导致编译错误，但不会崩溃
    }
    const auto data = res.uncompressedData();
    if (data.isEmpty()) {
        if (logger)
            logger->error("OpenGLCommon: Shader resource is empty: {}", fileName.toStdString());
        return shader;
    }
    shader.append(data.constData(), data.size());
    return shader;
}
