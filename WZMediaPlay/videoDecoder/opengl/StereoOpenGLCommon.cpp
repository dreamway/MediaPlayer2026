#include "StereoOpenGLCommon.hpp"
#include "Functions.hpp"
#include "OpenGLCommon.hpp"
#include "OpenGLVertices.hpp"
//#include "OpenGLHWInterop.hpp"
#include "DebugImageSaver.h"
#include "../Frame.h"
#include "../../GlobalDef.h"  // 包含 ENABLE_VIDEO_TRACE 宏定义

#include <spdlog/spdlog.h>
#include <QOpenGLShader>
#include <QOpenGLShaderProgram>
#include <QVector2D>
#include <QVector4D>
#include <QImage>
#include <QDir>
#include <QDateTime>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <QMatrix3x3>
#include <QResource>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <cstring>

extern spdlog::logger *logger;

StereoOpenGLCommon::StereoOpenGLCommon()
    : debugImageSaver_(nullptr)
{
#if ENABLE_VIDEO_TRACE
    // 创建异步图片保存器（使用 nullptr 作为父对象，因为 StereoOpenGLCommon 不是 QObject）
    // 注意：需要在析构函数中手动删除
    debugImageSaver_ = new DebugImageSaver(nullptr);
#else
    debugImageSaver_ = nullptr;
#endif
}

StereoOpenGLCommon::~StereoOpenGLCommon()
{
#if ENABLE_VIDEO_TRACE
    // 手动停止并删除 DebugImageSaver（因为没有父对象）
    if (debugImageSaver_) {
        debugImageSaver_->stop();
        delete debugImageSaver_;
        debugImageSaver_ = nullptr;
    }
#endif
}

void StereoOpenGLCommon::initializeStereoShader()
{
    if (m_stereoShaderInitialized)
        return;

    if (numPlanes == 1)
    {
        // RGB 格式暂不支持 3D（需要单独的 RGB 3D shader）
        if (logger) logger->warn("StereoOpenGLCommon: RGB format 3D shader not implemented yet");
        m_stereoShaderInitialized = true;
        return;
    }

    shaderProgramStereo.reset(new QOpenGLShaderProgram);

    // 从 ShaderManager 加载 shader（优先使用外部 shader，否则使用内部 shader）
    QString vertexSource, fragmentSource;
    
    // 先尝试加载外部 shader
    QFile vertexFile("./Shader/vertex.glsl");
    if (vertexFile.exists() && vertexFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream vertexIn(&vertexFile);
        vertexSource = vertexIn.readAll();
        vertexFile.close();
        if (logger) logger->info("StereoOpenGLCommon: Loaded external vertex shader");
    }
    
    QFile fragmentFile("./Shader/fragment.glsl");
    if (fragmentFile.exists() && fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream fragIn(&fragmentFile);
        fragmentSource = fragIn.readAll();
        fragmentFile.close();
        if (logger) logger->info("StereoOpenGLCommon: Loaded external fragment shader");
    }
    
    // 如果外部 shader 不存在，尝试加载内部 shader
    if (vertexSource.isEmpty())
    {
        QFile vertexFileInternal(":/MainWindow/Shader/vertex.glsl");
        if (vertexFileInternal.exists() && vertexFileInternal.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream vertexIn(&vertexFileInternal);
            vertexSource = vertexIn.readAll();
            vertexFileInternal.close();
            if (logger) logger->info("StereoOpenGLCommon: Loaded internal vertex shader");
        }
    }
    
    if (fragmentSource.isEmpty())
    {
        QFile fragmentFileInternal(":/MainWindow/Shader/fragment.glsl");
        if (fragmentFileInternal.exists() && fragmentFileInternal.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream fragIn(&fragmentFileInternal);
            fragmentSource = fragIn.readAll();
            fragmentFileInternal.close();
            if (logger) logger->info("StereoOpenGLCommon: Loaded internal fragment shader");
        }
    }
    
    // 检查 shader 是否加载成功
    if (vertexSource.isEmpty() || fragmentSource.isEmpty())
    {
        if (logger) logger->error("StereoOpenGLCommon: Failed to load shader sources (vertex: {}, fragment: {})", 
            vertexSource.isEmpty() ? "empty" : "ok", fragmentSource.isEmpty() ? "empty" : "ok");
        m_stereoShaderInitialized = false;
        return;
    }
    
    // 转换 QString 到 QByteArray
    QByteArray vertexShader = vertexSource.toUtf8();
    QByteArray stereoFrag = fragmentSource.toUtf8();
    
    // fragment.glsl 已经包含了所有必要的逻辑，不需要动态插入宏定义
    // 格式信息（NV12/YUV420P/RGB）通过 uniform sampler（textureY/textureU/textureV/textureUV）传递
    // OpenGL 版本和纹理类型信息由 shader 本身处理，不需要宏定义
    
    // 编译 shader
    if (!shaderProgramStereo->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader))
    {
        if (logger) logger->error("StereoOpenGLCommon: Vertex shader compile failed: {}", 
            shaderProgramStereo->log().toStdString());
        m_stereoShaderInitialized = false;
        return;
    }
    
    if (!shaderProgramStereo->addShaderFromSourceCode(QOpenGLShader::Fragment, stereoFrag))
    {
        QString errorLog = shaderProgramStereo->log();
        if (logger) {
            logger->error("StereoOpenGLCommon: Fragment shader compile failed: {}", errorLog.toStdString());
            // 输出 shader 源码的前500个字符用于调试
            logger->error("StereoOpenGLCommon: Fragment shader source (first 500 chars): {}", 
                QString::fromUtf8(stereoFrag.left(500)).toStdString());
        }
        m_stereoShaderInitialized = false;
        return;
    }
    
    if (!shaderProgramStereo->link())
    {
        if (logger) logger->error("StereoOpenGLCommon: Stereo shader link failed: {}", shaderProgramStereo->log().toStdString());
        m_stereoShaderInitialized = false;
        return;
    }
    
    if (shaderProgramStereo->bind())
    {
        // 获取 attribute locations（fragment.glsl 使用 aTexCoord，vertex.glsl 使用 aPos）
        texCoordYCbCrLoc = shaderProgramStereo->attributeLocation("aTexCoord");
        positionYCbCrLoc = shaderProgramStereo->attributeLocation("aPos");  // vertex.glsl 使用 aPos，不是 aPosition
        
        if (texCoordYCbCrLoc < 0 || positionYCbCrLoc < 0)
        {
            if (logger) logger->warn("StereoOpenGLCommon: Attribute locations - aTexCoord: {}, aPos: {}", 
                texCoordYCbCrLoc, positionYCbCrLoc);
        }
        
        // 设置纹理 uniform（fragment.glsl 使用 textureY, textureU, textureV, textureUV）
        // 这些 uniform 在每次 bind 时都需要设置，因为 shader 可能会被其他程序使用
        shaderProgramStereo->setUniformValue("textureY", 0);
        if (numPlanes == 2)
        {
            shaderProgramStereo->setUniformValue("textureUV", 1);
            // 对于 NV12 格式，textureU 和 textureV 不需要设置（或设置为 -1）
            shaderProgramStereo->setUniformValue("textureU", -1);
            shaderProgramStereo->setUniformValue("textureV", -1);
        }
        else if (numPlanes == 3)
        {
            shaderProgramStereo->setUniformValue("textureU", 1);
            shaderProgramStereo->setUniformValue("textureV", 2);
            // textureUV 不需要设置（或设置为 -1）
            shaderProgramStereo->setUniformValue("textureUV", -1);
        }
        else
        {
            // 单平面格式（RGB），只使用 textureY
            shaderProgramStereo->setUniformValue("textureU", -1);
            shaderProgramStereo->setUniformValue("textureV", -1);
            shaderProgramStereo->setUniformValue("textureUV", -1);
        }
        
        shaderProgramStereo->release();
    }
    else
    {
        if (logger) logger->error("StereoOpenGLCommon: Stereo shader bind failed");
        m_stereoShaderInitialized = false;
        return;
    }

    m_stereoShaderInitialized = true;
    if (logger) logger->info("StereoOpenGLCommon: Stereo shader initialized successfully");
}

void StereoOpenGLCommon::paintGL()
{
    static int paintGLCounter = 0;
    static int debugSaveCounter = 0;
    paintGLCounter++;
    
#if ENABLE_VIDEO_TRACE
    bool shouldSaveBefore = (paintGLCounter % 10 == 0);  // 每10次保存一次渲染前的帧（更频繁）
    bool shouldSaveAfter = (paintGLCounter % 100 == 0);  // 每100次保存一次渲染后的帧
#else
    bool shouldSaveBefore = false;
    bool shouldSaveAfter = false;
#endif
    
    // 在渲染前保存帧（如果启用且帧不为空）
#if ENABLE_VIDEO_TRACE
    if (shouldSaveBefore && !videoFrame.isEmpty() && debugImageSaver_)
    {
        AVPixelFormat srcFormat = videoFrame.pixelFormat();
        int width = videoFrame.width(0);
        int height = videoFrame.height(0);
        
        if (width > 0 && height > 0) {
            // 将 Frame 转换为 QImage（异步保存，不阻塞渲染）
            SwsContext* swsCtx = sws_getContext(
                width, height, srcFormat,
                width, height, AV_PIX_FMT_RGB24,
                SWS_BILINEAR, nullptr, nullptr, nullptr);
            
            if (swsCtx) {
                int rgbLinesize[1] = { width * 3 };
                uint8_t* rgbData[1] = { new uint8_t[width * height * 3] };
                
                const uint8_t* srcData[4] = {nullptr};
                int srcLinesize[4] = {0};
                
                if (srcFormat == AV_PIX_FMT_NV12 || srcFormat == AV_PIX_FMT_NV21) {
                    srcData[0] = videoFrame.constData(0);
                    srcData[1] = videoFrame.constData(1);
                    srcLinesize[0] = videoFrame.linesize(0);
                    srcLinesize[1] = videoFrame.linesize(1);
                } else {
                    srcData[0] = videoFrame.constData(0);
                    srcData[1] = videoFrame.constData(1);
                    srcData[2] = videoFrame.constData(2);
                    srcLinesize[0] = videoFrame.linesize(0);
                    srcLinesize[1] = videoFrame.linesize(1);
                    srcLinesize[2] = videoFrame.linesize(2);
                }
                
                int ret = sws_scale(swsCtx, srcData, srcLinesize, 0, height, rgbData, rgbLinesize);
                if (ret == height) {
                    QImage img(width, height, QImage::Format_RGB888);
                    for (int y = 0; y < height; ++y) {
                        memcpy(img.scanLine(y), rgbData[0] + y * rgbLinesize[0], width * 3);
                    }
                    
                    // 异步保存（不阻塞渲染）
                    QDateTime timestamp = QDateTime::currentDateTime();
                    QString filename = QString("debug_frames_stereo/frame_before_paintGL_%1_%2x%3_%4.png")
                        .arg(timestamp.toString("yyyyMMdd_HHmmss_zzz"))
                        .arg(width)
                        .arg(height)
                        .arg(debugSaveCounter++);
                    
                    debugImageSaver_->enqueueImage(img, filename);
                }
                
                delete[] rgbData[0];
                sws_freeContext(swsCtx);
            }
        }
    }
#endif  // ENABLE_VIDEO_TRACE
    
    // 视频文件渲染：统一使用 paintGLStereo() 进行渲染，因为它兼容 2D 和 3D 模式
    // paintGLStereo() 会根据 m_stereoFormat 和 shader 状态自动选择正确的渲染方式
    // 这样可以避免 OpenGLCommon::paintGL() 在 StereoOpenGLCommon 中的兼容性问题
    // 注意：Camera 渲染已移至独立的 CameraOpenGLWidget，不再在此处理
    if (logger && paintGLCounter % 100 == 0) {
        logger->debug("StereoOpenGLCommon::paintGL: Using paintGLStereo() for rendering (m_stereoFormat: {}, m_stereoShaderInitialized: {}, shaderProgramStereo: {})", 
            static_cast<int>(m_stereoFormat), m_stereoShaderInitialized, shaderProgramStereo != nullptr);
    }
    paintGLStereo();
    
    // 在渲染后保存 framebuffer（如果启用，使用异步保存）
#if ENABLE_VIDEO_TRACE
    if (shouldSaveAfter && hasImage && debugImageSaver_)
    {
        // 获取当前 viewport 尺寸
        GLint viewport[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        int width = viewport[2];
        int height = viewport[3];
        
        if (width > 0 && height > 0)
        {
            // 从 framebuffer 读取像素
            QImage img(width, height, QImage::Format_RGB888);
            glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, img.bits());
            
            // OpenGL 的坐标系是左下角为原点，QImage 是左上角，需要翻转
            img = img.mirrored(false, true);
            
            // 异步保存（不阻塞渲染）
            QDateTime timestamp = QDateTime::currentDateTime();
            QString filename = QString("debug_frames_stereo/frame_after_paintGL_%1_%2x%3_%4.png")
                .arg(timestamp.toString("yyyyMMdd_HHmmss_zzz"))
                .arg(width)
                .arg(height)
                .arg(debugSaveCounter++);
            
            debugImageSaver_->enqueueImage(img, filename);
        }
    }
#endif  // ENABLE_VIDEO_TRACE
}

void StereoOpenGLCommon::paintGLStereo()
{
    // 复用父类的纹理上传逻辑，但使用 stereo shader 绘制
    // 简化设计：不再检查硬件帧，直接使用软件帧
    const bool frameIsEmpty = videoFrame.isEmpty();

    static int paintGLStereoCounter = 0;
    paintGLStereoCounter++;
    
    if (logger && paintGLStereoCounter % 100 == 0) {
        logger->debug("StereoOpenGLCommon::paintGLStereo: Called {} times, frameIsEmpty: {}, videoFrame.isEmpty(): {}, hasImage: {}, m_stereoFormat: {}, m_stereoShaderInitialized: {}", 
            paintGLStereoCounter, frameIsEmpty, videoFrame.isEmpty(), hasImage, 
            static_cast<int>(m_stereoFormat), m_stereoShaderInitialized);
        if (!videoFrame.isEmpty()) {
            logger->debug("StereoOpenGLCommon::paintGLStereo: videoFrame - width: {}, height: {}, planes: {}, isHW: {}", 
                videoFrame.width(0), videoFrame.height(0), videoFrame.numPlanes(), videoFrame.isHW());
        }
    }

    if (updateTimer.isActive())
        updateTimer.stop();

    // 无新帧且从未渲染过才返回；无新帧但 hasImage 时继续用已有纹理绘制，避免黑屏闪烁（BUG 6）
    if (frameIsEmpty && !hasImage)
    {
        if (logger && paintGLStereoCounter % 100 == 0) {
            logger->debug("StereoOpenGLCommon::paintGLStereo: Early return - frameIsEmpty: {}, hasImage: {}", frameIsEmpty, hasImage);
        }
        return;
    }

    bool resetDone = false;

    // 复用父类的纹理上传逻辑（从 OpenGLCommon::paintGL 复制）
    if (!frameIsEmpty)
    {
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

        if (doReset)
        {
            // 简化设计：不再使用硬件互操作，直接使用软件帧路径
            const qint32 halfLinesize = (videoFrame.linesize(0) >> videoFrame.chromaShiftW());
            correctLinesize =
            (
                (halfLinesize == videoFrame.linesize(1) && videoFrame.linesize(1) == videoFrame.linesize(2)) &&
                (videoFrame.linesize(1) == halfLinesize)
            );
            for (qint32 p = 0; p < 3; ++p)
            {
                const GLsizei w = correctLinesize ? videoFrame.linesize(p) / bytesMultiplier : widths[p];
                const GLsizei h = heights[p];
                if (p == 0)
                    m_textureSize = QSize(w, h);
                if (hasPbo)
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[p + 1]);
                    glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * bytesMultiplier, nullptr, GL_DYNAMIC_DRAW);
                }
                glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
                glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, w, h, 0, fmt, dataType, nullptr);
            }
            texCoordYCbCr[2] = texCoordYCbCr[6] = (videoFrame.linesize(0) / bytesMultiplier == widths[0]) ? 1.0f : (widths[0] / (videoFrame.linesize(0) / bytesMultiplier + 1.0f));
            resetDone = true;
            hasImage = false;
        }

        // 上传纹理数据（简化设计：不再使用硬件互操作）
        {
            for (qint32 p = 0; p < 3; ++p)
            {
                const quint8 *data = videoFrame.constData(p);
                const GLsizei w = correctLinesize ? videoFrame.linesize(p) / bytesMultiplier : widths[p];
                const GLsizei h = heights[p];
                if (hasPbo)
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[p + 1]);
                    quint8 *dst = nullptr;
                    // 简化设计：假设支持 mapBufferRange
                    if (true)  // 总是使用 mapBufferRange
                        dst = (quint8 *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, w * h * bytesMultiplier, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#if !defined(QT_OPENGL_ES_2) && !defined(QT_FEATURE_opengles2)
                    else
                        dst = (quint8 *)m_gl15.glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#endif
                    if (!dst)
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                    else
                    {
                        if (correctLinesize)
                            memcpy(dst, data, w * h * bytesMultiplier);
                        else for (int y = 0; y < h; ++y)
                        {
                            memcpy(dst, data, w * bytesMultiplier);
                            data += videoFrame.linesize(p);
                            dst  += w * bytesMultiplier;
                        }
                        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                        data = nullptr;
                    }
                }
                glActiveTexture(GL_TEXTURE0 + p);
                glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
                if (hasPbo || correctLinesize)
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, fmt, dataType, data);
                else for (int y = 0; y < h; ++y)
                {
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, w, 1, fmt, dataType, data);
                    data += videoFrame.linesize(p);
                }
            }
            if (hasPbo)
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }

        // 清除帧数据以释放内存（避免队列满）
        // 对于硬件解码，如果不是 copy 模式，数据已经在 GPU 上，可以清除 CPU 端的数据
        // 对于软件解码，数据已经上传到纹理，可以清除
        // 简化设计：总是清除帧数据（已上传到纹理）
        if (true)
        {
            videoFrame.clear();
        }
        else
        {
            // 硬件解码且不是 copy 模式：数据在 GPU 上，但 Frame 对象可能还持有引用
            // 为了释放内存，仍然清除 Frame（硬件纹理已经绑定，不需要 CPU 数据）
            videoFrame.clear();
        }
        hasImage = true;
    }

    // 使用 stereo shader 绘制
    // 注意：纹理数据已经在上面的代码中上传并绑定到 GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2
    if (!shaderProgramStereo || !hasImage)
    {
        if (logger && paintGLStereoCounter % 100 == 0) {
            logger->debug("StereoOpenGLCommon::paintGLStereo: Cannot render - shaderProgramStereo: {}, hasImage: {}", 
                shaderProgramStereo != nullptr, hasImage);
        }
        return;
    }

    // vertex.glsl 使用 vec2 aPos，直接使用 2D 坐标
    shaderProgramStereo->setAttributeArray(positionYCbCrLoc, verticesYCbCr[verticesIdx], 2);
    shaderProgramStereo->setAttributeArray(texCoordYCbCrLoc, texCoordYCbCr, 2);
    
    shaderProgramStereo->enableAttributeArray(positionYCbCrLoc);
    shaderProgramStereo->enableAttributeArray(texCoordYCbCrLoc);

    if (!shaderProgramStereo->bind())
    {
        if (logger) logger->error("StereoOpenGLCommon::paintGLStereo: Failed to bind shader program");
        return;
    }
    
    // 每次渲染时都设置 uniform（不只是 doReset 时）
    // 因为 uniform 值可能会在渲染过程中改变（例如通过 UI 调整）
    setStereoShaderUniforms();
    
    // 确保纹理已正确绑定到对应的纹理单元
    // 纹理数据应该已经在上面上传了，这里只需要确保绑定状态正确
    // 注意：对于 YUV420P，numPlanes = 3（Y, U, V），但 fragment.glsl 使用 textureY, textureU, textureV
    // 简化设计：不再使用硬件互操作，总是使用软件纹理
    {
        // 软件解码：确保纹理绑定到正确的纹理单元
        // YUV420P: numPlanes = 3, 纹理索引为 textures[1], textures[2], textures[3]
        // 对应 GL_TEXTURE0, GL_TEXTURE1, GL_TEXTURE2
        for (qint32 p = 0; p < numPlanes && p < 3; ++p)
        {
            glActiveTexture(GL_TEXTURE0 + p);
            glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
            if (logger && paintGLStereoCounter % 100 == 0) {
                logger->debug("StereoOpenGLCommon::paintGLStereo: Bound texture {} to GL_TEXTURE{} (plane {})",
                    textures[p + 1], p, p);
            }
        }
    }
    // 简化设计：不再使用硬件互操作，纹理已在上面绑定

    // 关键修复：每次渲染时更新纹理 uniform（因为 numPlanes 可能在初始化后改变）
    // 这解决了 stereo shader 在 numPlanes=0 时初始化导致纹理 uniform 设置错误的问题
    shaderProgramStereo->setUniformValue("textureY", 0);
    if (numPlanes == 2)
    {
        shaderProgramStereo->setUniformValue("textureUV", 1);
        shaderProgramStereo->setUniformValue("textureU", -1);
        shaderProgramStereo->setUniformValue("textureV", -1);
    }
    else if (numPlanes == 3)
    {
        shaderProgramStereo->setUniformValue("textureU", 1);
        shaderProgramStereo->setUniformValue("textureV", 2);
        shaderProgramStereo->setUniformValue("textureUV", -1);
    }
    else
    {
        // 单平面格式（RGB），只使用 textureY
        shaderProgramStereo->setUniformValue("textureU", -1);
        shaderProgramStereo->setUniformValue("textureV", -1);
        shaderProgramStereo->setUniformValue("textureUV", -1);
    }
    
    // fragment.glsl 不使用 uMatrix，vertex.glsl 也不使用，所以不需要设置
    // 如果需要矩阵变换，可以在 vertex shader 中添加
    
    // 确保 viewport 设置正确（应该在 initializeGL 或 resizeGL 中设置，但这里也检查一下）
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) {
        if (logger && paintGLStereoCounter % 100 == 0) {
            logger->warn("StereoOpenGLCommon::paintGLStereo: Invalid viewport: {}x{}, widget size: {}x{}", 
                viewport[2], viewport[3], 
                m_widget ? m_widget->width() : -1, m_widget ? m_widget->height() : -1);
        }
        // 如果 viewport 无效，尝试从 widget 尺寸设置
        if (m_widget && m_widget->width() > 0 && m_widget->height() > 0) {
            glViewport(0, 0, m_widget->width(), m_widget->height());
            if (logger && paintGLStereoCounter % 100 == 0) {
                logger->info("StereoOpenGLCommon::paintGLStereo: Set viewport to widget size: {}x{}", 
                    m_widget->width(), m_widget->height());
            }
        }
    }
    
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    if (logger && paintGLStereoCounter % 100 == 0) {
        logger->debug("StereoOpenGLCommon::paintGLStereo: glDrawArrays completed");
    }
   
    shaderProgramStereo->release();

    shaderProgramStereo->disableAttributeArray(texCoordYCbCrLoc);
    shaderProgramStereo->disableAttributeArray(positionYCbCrLoc);
}

void StereoOpenGLCommon::setStereoShaderUniforms()
{
    if (!shaderProgramStereo)
        return;
    
    // 注意：这个方法应该在 shader 已经 bind 的情况下调用
    // 调用者（paintGLStereo）已经 bind 了 shader，所以这里不需要再次 bind

    // 设置 fragment.glsl 中使用的 uniform（只设置实际存在的）
    // iRenderInputSource: 0 = VideoFile, 1 = Camera（暂时固定为 0，因为 Camera 渲染由 CameraRenderer 处理）
    shaderProgramStereo->setUniformValue("iRenderInputSource", 0);
    
    // 3D uniform 参数
    shaderProgramStereo->setUniformValue("iStereoFlag", m_stereoFormat == STEREO_FORMAT_3D ? 1 : 0);
    shaderProgramStereo->setUniformValue("iStereoInputFormat", static_cast<int>(m_stereoInputFormat));
    shaderProgramStereo->setUniformValue("iStereoOutputFormat", static_cast<int>(m_stereoOutputFormat));
    shaderProgramStereo->setUniformValue("bEnableRegion", m_enableStereoRegion);
    shaderProgramStereo->setUniformValue("VecRegion", QVector4D(m_stereoRegion[0], m_stereoRegion[1], m_stereoRegion[2], m_stereoRegion[3]));
    shaderProgramStereo->setUniformValue("iParallaxOffset", m_parallaxShift);
    
    // 注意：还原后的shader使用硬编码的YUV到RGB转换系数，不再需要iVideoFormat和uYUVtRGB uniform
    
    // 检查 uniform 设置是否成功（用于调试，每100次检查一次）
    static int uniformCheckCounter = 0;
    uniformCheckCounter++;
    if (logger && uniformCheckCounter % 100 == 0) {
        int uniformLoc = shaderProgramStereo->uniformLocation("iStereoFlag");
        if (uniformLoc < 0) {
            logger->warn("StereoOpenGLCommon::setStereoShaderUniforms: Uniform iStereoFlag not found in shader");
        }
        uniformLoc = shaderProgramStereo->uniformLocation("textureY");
        if (uniformLoc < 0) {
            logger->warn("StereoOpenGLCommon::setStereoShaderUniforms: Uniform textureY not found in shader");
        } else {
            logger->debug("StereoOpenGLCommon::setStereoShaderUniforms: Uniform textureY location: {}", uniformLoc);
        }
    }
}

void StereoOpenGLCommon::setStereoFormat(StereoFormat format)
{
    m_stereoFormat = format;
    doReset = true;
}

void StereoOpenGLCommon::setStereoInputFormat(StereoInputFormat inputFormat)
{
    m_stereoInputFormat = inputFormat;
    doReset = true;
}

void StereoOpenGLCommon::setStereoOutputFormat(StereoOutputFormat outputFormat)
{
    m_stereoOutputFormat = outputFormat;
    doReset = true;
}

void StereoOpenGLCommon::setParallaxShift(int shift)
{
    m_parallaxShift = shift;
    doReset = true;
}

void StereoOpenGLCommon::setStereoEnableRegion(bool enable, float topLeftX, float topLeftY, float bottomRightX, float bottomRightY)
{
    m_enableStereoRegion = enable;
    m_stereoRegion[0] = topLeftX;
    m_stereoRegion[1] = topLeftY;
    m_stereoRegion[2] = bottomRightX;
    m_stereoRegion[3] = bottomRightY;
    doReset = true;
}
