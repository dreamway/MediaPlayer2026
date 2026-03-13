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
#include <QMutexLocker>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <QMatrix3x3>
#include <QResource>
#include <QFile>
#include <QTextStream>
#include <QIODevice>
#include <cstring>
#include <cmath>  // for std::abs

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

    // 删除 VBO 和 EBO
    if (m_vbo != 0) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_ebo != 0) {
        glDeleteBuffers(1, &m_ebo);
        m_ebo = 0;
    }
    m_vboInitialized = false;
}

void StereoOpenGLCommon::initializeStereoShader()
{
    if (m_stereoShaderInitialized)
        return;

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

    // 创建并绑定 VAO (Vertex Array Object) - OpenGL Core Profile 必需
    if (!m_vao.create()) {
        if (logger) logger->error("StereoOpenGLCommon: Failed to create VAO");
        m_stereoShaderInitialized = false;
        return;
    }
    m_vao.bind();
    if (logger) logger->info("StereoOpenGLCommon: VAO created and bound successfully");

    // === 初始化默认顶点数据（参考v1.0.8 FFmpegView.h:207-212） ===
    // 交错数组格式：每个顶点5个float {x, y, z, tex_x, tex_y}
    // 顶点顺序：右上, 右下, 左下, 左上（与v1.0.8一致）
    m_vertices[0]  =  1.0f; m_vertices[1]  =  1.0f; m_vertices[2]  = 0.0f;  // 右上 位置
    m_vertices[3]  =  1.0f; m_vertices[4]  = 0.0f;                         // 右上 纹理
    m_vertices[5]  =  1.0f; m_vertices[6]  = -1.0f; m_vertices[7]  = 0.0f;  // 右下 位置
    m_vertices[8]  =  1.0f; m_vertices[9]  = 1.0f;                         // 右下 纹理
    m_vertices[10] = -1.0f; m_vertices[11] = -1.0f; m_vertices[12] = 0.0f;  // 左下 位置
    m_vertices[13] =  0.0f; m_vertices[14] = 1.0f;                         // 左下 纹理
    m_vertices[15] = -1.0f; m_vertices[16] =  1.0f; m_vertices[17] = 0.0f;  // 左上 位置
    m_vertices[18] =  0.0f; m_vertices[19] = 0.0f;                         // 左上 纹理

    // 创建 VBO (Vertex Buffer Object) - 交错数组
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(m_vertices), m_vertices, GL_DYNAMIC_DRAW);

    // 创建 EBO (Element Buffer Object) - 索引数据
    glGenBuffers(1, &m_ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_indices), m_indices, GL_STATIC_DRAW);

    // 设置顶点属性指针（参考v1.0.8 FFmpegView.cc:667-670）
    // location 0: 位置属性 (vec3, 步长5个float, 偏移0)
    glEnableVertexAttribArray(positionYCbCrLoc);
    glVertexAttribPointer(positionYCbCrLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);

    // location 1: 纹理坐标属性 (vec2, 步长5个float, 偏移3个float)
    glEnableVertexAttribArray(texCoordYCbCrLoc);
    glVertexAttribPointer(texCoordYCbCrLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_vao.release();  // 解绑 VAO，保存所有顶点属性状态
    m_vboInitialized = true;

    if (logger) logger->info("StereoOpenGLCommon: VBO and EBO created successfully (vbo: {}, ebo: {})",
        m_vbo, m_ebo);

    m_stereoShaderInitialized = true;
    if (logger) logger->info("StereoOpenGLCommon: Stereo shader initialized successfully");
}

void StereoOpenGLCommon::paintGL()
{
    static int paintGLCounter = 0;
    static int debugSaveCounter = 0;
    paintGLCounter++;

#if ENABLE_VIDEO_TRACE
    // 更频繁地保存调试帧用于分析
    bool shouldSaveBefore = (paintGLCounter % 5 == 0);  // 每5次保存一次
    bool shouldSaveAfter = (paintGLCounter % 10 == 0);  // 每10次保存一次渲染后的帧
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
    // BUG-039 修复：使用互斥锁保护帧数据访问
    // 避免在渲染时帧数据被其他线程修改导致崩溃
    // 在锁的保护下复制帧数据，然后使用副本进行渲染
    Frame localFrame;
    {
        QMutexLocker locker(&videoFrameMutex);
        localFrame = videoFrame;
    }

    // 复用父类的纹理上传逻辑，但使用 stereo shader 绘制
    // 简化设计：不再检查硬件帧，直接使用软件帧
    const bool frameIsEmpty = localFrame.isEmpty();

    static int paintGLStereoCounter = 0;
    paintGLStereoCounter++;

    // 输出 info 级别日志来确认 paintGLStereo 被调用（每100次输出一次）
    if (logger && paintGLStereoCounter % 100 == 0) {
        logger->info("StereoOpenGLCommon::paintGLStereo: Called {} times, frameIsEmpty: {}, hasImage: {}",
            paintGLStereoCounter, frameIsEmpty, hasImage);
    }

    if (logger && paintGLStereoCounter % 100 == 0) {
        logger->debug("StereoOpenGLCommon::paintGLStereo: Called {} times, frameIsEmpty: {}, localFrame.isEmpty(): {}, hasImage: {}, m_stereoFormat: {}, m_stereoShaderInitialized: {}",
            paintGLStereoCounter, frameIsEmpty, localFrame.isEmpty(), hasImage,
            static_cast<int>(m_stereoFormat), m_stereoShaderInitialized);
        if (!localFrame.isEmpty()) {
            logger->debug("StereoOpenGLCommon::paintGLStereo: localFrame - width: {}, height: {}, planes: {}, isHW: {}",
                localFrame.width(0), localFrame.height(0), localFrame.numPlanes(), localFrame.isHW());
        }
    }

    if (updateTimer.isActive())
        updateTimer.stop();

    // 无新帧且从未渲染过才返回；无新帧但 hasImage 时继续用已有纹理绘制，避免黑屏闪烁（BUG 6）
    if (frameIsEmpty && !hasImage)
    {
        // BUG-034 修复：在返回前清除 OpenGL 缓冲区，避免旧帧残留在背景中
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        if (logger && paintGLStereoCounter % 100 == 0) {
            logger->debug("StereoOpenGLCommon::paintGLStereo: Early return - frameIsEmpty: {}, hasImage: {}, cleared buffer", frameIsEmpty, hasImage);
        }
        return;
    }

    bool resetDone = false;

    // 复用父类的纹理上传逻辑（从 OpenGLCommon::paintGL 复制）
    if (!frameIsEmpty)
    {
        // 根据实际帧格式动态设置 numPlanes
        numPlanes = localFrame.numPlanes();
        if (numPlanes < 2) numPlanes = 3;  // 默认 YUV420P

        const GLsizei widths[3] = {
            localFrame.width(0),
            localFrame.width(1),
            localFrame.width(2),
        };
        const GLsizei heights[3] = {
            localFrame.height(0),
            localFrame.height(1),
            localFrame.height(2),
        };

        // 诊断日志
        if (logger && paintGLStereoCounter % 10 == 0) {
            logger->info("StereoOpenGLCommon::paintGLStereo: Frame info - numPlanes: {}, Y: {}x{}, U: {}x{}, V: {}x{}, linesize: {}/{}/{}",
                numPlanes, widths[0], heights[0], widths[1], heights[1], widths[2], heights[2],
                localFrame.linesize(0), localFrame.linesize(1), localFrame.linesize(2));
        }
        const int bytesMultiplier = (m_depth + 7) / 8;
        const GLenum dataType = (bytesMultiplier == 1) ? GL_UNSIGNED_BYTE : GL_UNSIGNED_SHORT;

        // NV12 格式的 UV 平面需要使用 GL_RG8 格式（双通道）
        // YUV420P 格式使用 GL_R8 格式（单通道）
        // 注意：NV12 有 2 个平面，YUV420P 有 3 个平面

        if (doReset)
        {
            // 简化设计：不再使用硬件互操作，直接使用软件帧路径
            const qint32 halfLinesize = (localFrame.linesize(0) >> localFrame.chromaShiftW());

            // NV12 格式的 UV 平面 linesize 计算说明：
            // - Y 平面：linesize(0) = width（每像素 1 字节）
            // - UV 平面：linesize(1) = width（每 UV 对 2 字节，但 linesize 以字节为单位）
            // - halfLinesize = width / 2（以像素为单位的 UV 宽度）
            // - NV12 的 UV 平面 linesize 应该等于 Y 的 linesize，因为每个 UV 对占 2 字节

            if (numPlanes >= 3) {
                // YUV420P 格式：U 和 V 是独立的平面
                correctLinesize =
                (
                    (halfLinesize == localFrame.linesize(1) && localFrame.linesize(1) == localFrame.linesize(2)) &&
                    (localFrame.linesize(1) == halfLinesize)
                );
            } else {
                // NV12 格式：UV 是交织的单平面
                // NV12 的 UV 平面 linesize(1) 应该等于 Y 平面的 linesize(0)
                // 因为每个 UV 对占 2 字节，对应 2 个水平像素
                // 这意味着 correctLinesize 应该用不同的逻辑
                // 对于 NV12，linesize 匹配，但宽度是 Y 的一半
                correctLinesize = (localFrame.linesize(0) == localFrame.linesize(1));
            }

            const bool isNV12 = (numPlanes == 2);

            for (qint32 p = 0; p < numPlanes && p < 3; ++p)
            {
                // NV12 格式的 UV 平面纹理宽度 = widths[p]（像素宽度，已经是 Y 宽度的一半）
                // 不应该用 linesize 来计算，因为 NV12 的 linesize 是字节宽度
                const GLsizei w = widths[p];
                const GLsizei h = heights[p];
                if (p == 0)
                    m_textureSize = QSize(w, h);

                // NV12 格式的 UV 平面（p == 1 且 numPlanes == 2）需要使用 GL_RG8 格式
                // 因为 UV 是交织的双通道数据
                const bool isNV12UV = isNV12 && (p == 1);
                const GLint planeInternalFmt = isNV12UV ? GL_RG8 : GL_R8;
                const GLenum planeFmt = isNV12UV ? GL_RG : GL_RED;
                const int planeBytesPerPixel = isNV12UV ? 2 : 1;

                if (hasPbo)
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[p + 1]);
                    // NV12 UV 平面：w * h * 2 字节
                    glBufferData(GL_PIXEL_UNPACK_BUFFER, w * h * planeBytesPerPixel, nullptr, GL_DYNAMIC_DRAW);
                }
                glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
                glTexImage2D(GL_TEXTURE_2D, 0, planeInternalFmt, w, h, 0, planeFmt, GL_UNSIGNED_BYTE, nullptr);
                // 设置纹理参数
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            texCoordYCbCr[2] = texCoordYCbCr[6] = (localFrame.linesize(0) == widths[0]) ? 1.0f : (widths[0] / (localFrame.linesize(0) + 1.0f));
            resetDone = true;
            hasImage = false;
        }

        // 上传纹理数据（简化设计：不再使用硬件互操作）
        {
            const bool isNV12 = (numPlanes == 2);

            for (qint32 p = 0; p < numPlanes && p < 3; ++p)
            {
                const quint8 *data = localFrame.constData(p);
                // NV12 的 UV 平面纹理宽度 = widths[p]（像素宽度）
                // NV12 的 UV 平面 linesize = widths[0]（字节宽度，因为每个 UV 对占 2 字节）
                const GLsizei texWidth = widths[p];
                const GLsizei h = heights[p];

                // NV12 格式的 UV 平面（p == 1 且 numPlanes == 2）需要使用 GL_RG 格式
                const bool isNV12UV = isNV12 && (p == 1);
                const GLenum planeFmt = isNV12UV ? GL_RG : GL_RED;
                const int planeBytesPerPixel = isNV12UV ? 2 : 1;

                // 计算源数据的行宽度（字节）
                // 对于 NV12 UV 平面，linesize(1) 是字节宽度（= Y 宽度），但纹理宽度是 Y 宽度的一半
                const GLsizei srcLineSize = localFrame.linesize(p);
                const GLsizei dstLineSize = texWidth * planeBytesPerPixel;

                if (hasPbo)
                {
                    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo[p + 1]);
                    quint8 *dst = nullptr;
                    // 简化设计：假设支持 mapBufferRange
                    if (true)  // 总是使用 mapBufferRange
                        dst = (quint8 *)glMapBufferRange(GL_PIXEL_UNPACK_BUFFER, 0, texWidth * h * planeBytesPerPixel, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
#if !defined(QT_OPENGL_ES_2) && !defined(QT_FEATURE_opengles2)
                    else
                        dst = (quint8 *)m_gl15.glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
#endif
                    if (!dst)
                        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                    else
                    {
                        // 检查是否需要逐行复制
                        // NV12 UV 平面：srcLineSize = Y_width, dstLineSize = UV_width * 2 = Y_width / 2 * 2 = Y_width
                        // 所以 NV12 UV 平面的 srcLineSize == dstLineSize，可以一次性复制
                        if (srcLineSize == dstLineSize)
                        {
                            memcpy(dst, data, dstLineSize * h);
                        }
                        else
                        {
                            // 逐行复制
                            for (int y = 0; y < h; ++y)
                            {
                                memcpy(dst, data, dstLineSize);
                                data += srcLineSize;
                                dst  += dstLineSize;
                            }
                        }
                        glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                        data = nullptr;
                    }
                }
                glActiveTexture(GL_TEXTURE0 + p);
                glBindTexture(GL_TEXTURE_2D, textures[p + 1]);
                if (hasPbo)
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, h, planeFmt, GL_UNSIGNED_BYTE, data);
                else
                {
                    // 无 PBO 时，需要检查是否逐行复制
                    if (srcLineSize == dstLineSize)
                    {
                        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texWidth, h, planeFmt, GL_UNSIGNED_BYTE, data);
                    }
                    else
                    {
                        for (int y = 0; y < h; ++y)
                        {
                            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, texWidth, 1, planeFmt, GL_UNSIGNED_BYTE, data);
                            data += srcLineSize;
                        }
                    }
                }
            }
            if (hasPbo)
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        }

        // 清除帧数据以释放内存（避免队列满）
        // 对于硬件解码，如果不是 copy 模式，数据已经在 GPU 上，可以清除 CPU 端的数据
        // 对于软件解码，数据已经上传到纹理，可以清除
        // 简化设计：总是清除帧数据（已上传到纹理）

        // 在清除帧之前保存帧高度（用于颜色空间猜测）
        m_lastFrameHeight = localFrame.isEmpty() ? 0 : localFrame.height(0);

        // 清除原始帧数据（需要加锁）
        {
            QMutexLocker locker(&videoFrameMutex);
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

    // 绑定 VAO (Vertex Array Object) - OpenGL Core Profile 必需
    if (m_vao.isCreated()) {
        m_vao.bind();
    } else {
        if (logger && paintGLStereoCounter % 100 == 0) {
            logger->error("StereoOpenGLCommon::paintGLStereo: VAO not created");
        }
        return;
    }

    // vertex.glsl 使用 vec3 aPos（与v1.0.8一致）
    // 诊断：检查 attribute location 是否有效
    if (logger && paintGLStereoCounter % 100 == 0) {
        logger->info("StereoOpenGLCommon::paintGLStereo: Attribute locations - positionYCbCrLoc: {}, texCoordYCbCrLoc: {}",
            positionYCbCrLoc, texCoordYCbCrLoc);
    }

    // === 动态顶点计算（参考v1.0.8 FFmpegView::programDraw）===
    // 在绘制前根据视频宽高比动态调整顶点坐标
    if (m_textureSize.isValid()) {
        updateDynamicVertices(m_textureSize.width(), m_textureSize.height());
    } else if (videoFrame.width(0) > 0 && videoFrame.height(0) > 0) {
        // 如果m_textureSize无效，使用videoFrame尺寸
        updateDynamicVertices(videoFrame.width(0), videoFrame.height(0));
    }

    if (!shaderProgramStereo->bind())
    {
        if (logger) logger->error("StereoOpenGLCommon::paintGLStereo: Failed to bind shader program");
        return;
    }

    // 诊断：检查 shader 绑定后的 OpenGL 错误
    GLenum err1 = glGetError();
    if (err1 != GL_NO_ERROR && logger && paintGLStereoCounter % 100 == 0) {
        logger->error("StereoOpenGLCommon::paintGLStereo: OpenGL error after shader bind: 0x{:X}", err1);
    }

    // 每次渲染时都设置 uniform（不只是 doReset 时）
    // 因为 uniform 值可能会在渲染过程中改变（例如通过 UI 调整）
    setStereoShaderUniforms();

    // 诊断：检查设置 uniform 后的 OpenGL 错误
    GLenum err2 = glGetError();
    if (err2 != GL_NO_ERROR && logger && paintGLStereoCounter % 100 == 0) {
        logger->error("StereoOpenGLCommon::paintGLStereo: OpenGL error after setStereoShaderUniforms: 0x{:X}", err2);
    }
    
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
        // 注意：不要设置 sampler uniform 为 -1，这可能导致 OpenGL 错误
        // 对于 NV12 格式，textureU 和 textureV 不会被采样，可以不设置或设置为有效值
        shaderProgramStereo->setUniformValue("textureU", 1);  // 设置为有效的纹理单元
        shaderProgramStereo->setUniformValue("textureV", 2);  // 设置为有效的纹理单元
    }
    else if (numPlanes == 3)
    {
        shaderProgramStereo->setUniformValue("textureU", 1);
        shaderProgramStereo->setUniformValue("textureV", 2);
        shaderProgramStereo->setUniformValue("textureUV", 0);  // 设置为有效的纹理单元
    }
    else
    {
        // 单平面格式（RGB），只使用 textureY
        shaderProgramStereo->setUniformValue("textureU", 0);
        shaderProgramStereo->setUniformValue("textureV", 0);
        shaderProgramStereo->setUniformValue("textureUV", 0);
    }

    // 诊断：检查设置纹理 uniform 后的 OpenGL 错误
    GLenum err3 = glGetError();
    if (err3 != GL_NO_ERROR && logger && paintGLStereoCounter % 100 == 0) {
        logger->error("StereoOpenGLCommon::paintGLStereo: OpenGL error after texture uniforms: 0x{:X}", err3);
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

    // 使用 glDrawElements + EBO 绘制（与v1.0.8一致）
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    // 检查 OpenGL 错误 - 详细版本
    GLenum glError = glGetError();
    if (glError != GL_NO_ERROR && logger && paintGLStereoCounter % 100 == 0) {
        QString errorStr;
        switch (glError) {
            case GL_INVALID_ENUM: errorStr = "GL_INVALID_ENUM"; break;
            case GL_INVALID_VALUE: errorStr = "GL_INVALID_VALUE"; break;
            case GL_INVALID_OPERATION: errorStr = "GL_INVALID_OPERATION"; break;
            case GL_OUT_OF_MEMORY: errorStr = "GL_OUT_OF_MEMORY"; break;
            default: errorStr = QString("Unknown: 0x%1").arg(glError, 4, 16, QChar('0')); break;
        }
        logger->error("StereoOpenGLCommon::paintGLStereo: OpenGL error after glDrawElements: {} (0x{:X})",
            errorStr.toStdString(), glError);
    }

    if (logger && paintGLStereoCounter % 100 == 0) {
        logger->info("StereoOpenGLCommon::paintGLStereo: glDrawElements completed, numPlanes: {}, viewport: {}x{}, textures: {}/{}/{}",
            numPlanes, viewport[2], viewport[3], textures[1], textures[2], textures[3]);
    }

    shaderProgramStereo->release();

    // 释放 VAO（顶点属性状态存储在 VAO 中，不需要单独清理）
    m_vao.release();
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

    // 设置视频格式 uniform：0=RGB, 1=NV12, 2=YUV420P
    int videoFormat = 0; // 默认 RGB
    if (numPlanes == 2) {
        videoFormat = 1; // NV12
    } else if (numPlanes == 3) {
        videoFormat = 2; // YUV420P
    }
    shaderProgramStereo->setUniformValue("iVideoFormat", videoFormat);

    // 3D uniform 参数
    shaderProgramStereo->setUniformValue("iStereoFlag", m_stereoFormat == STEREO_FORMAT_3D ? 1 : 0);
    shaderProgramStereo->setUniformValue("iStereoInputFormat", static_cast<int>(m_stereoInputFormat));
    shaderProgramStereo->setUniformValue("iStereoOutputFormat", static_cast<int>(m_stereoOutputFormat));
    shaderProgramStereo->setUniformValue("bEnableRegion", m_enableStereoRegion);
    shaderProgramStereo->setUniformValue("VecRegion", QVector4D(m_stereoRegion[0], m_stereoRegion[1], m_stereoRegion[2], m_stereoRegion[3]));
    shaderProgramStereo->setUniformValue("iParallaxOffset", m_parallaxShift);

    // 设置颜色空间转换 uniform（YUV 到 RGB）
    if (videoFormat != 0) {
        // YUV 到 RGB 转换矩阵
        // 使用保存的帧高度（在清除帧之前保存），用于在颜色空间未指定时根据分辨率猜测正确的颜色空间
        const int frameHeight = m_lastFrameHeight;
        const QMatrix3x3 mat = Functions::getYUVtoRGBmatrix(m_colorSpace, frameHeight);
        shaderProgramStereo->setUniformValue("uYUVtRGB", mat);

        // 调试日志：每10帧记录一次颜色空间选择
        static int colorSpaceLogCounter = 0;
        colorSpaceLogCounter++;
        if (logger && colorSpaceLogCounter % 10 == 0) {
            const char* guessedSpace = m_colorSpace == AVCOL_SPC_UNSPECIFIED ?
                (frameHeight > 0 && frameHeight <= 576 ? "BT.601 (guessed from SD)" :
                 (frameHeight >= 2160 ? "BT.2020 (guessed from UHD)" : "BT.709 (guessed from HD)")) :
                "specified";
            logger->info("StereoOpenGLCommon::setStereoShaderUniforms: Color space: {} (source={}), frameHeight={}",
                        static_cast<int>(m_colorSpace), guessedSpace, frameHeight);
        }
    }

    // 检查 uniform 设置是否成功（用于调试，每100次检查一次）
    static int uniformCheckCounter = 0;
    uniformCheckCounter++;
    if (logger && uniformCheckCounter % 10 == 0) {
        int uniformLoc = shaderProgramStereo->uniformLocation("iStereoFlag");
        if (uniformLoc < 0) {
            logger->warn("StereoOpenGLCommon::setStereoShaderUniforms: Uniform iStereoFlag not found in shader");
        } else {
            // 获取实际的 uniform 值
            int stereoFlagValue = 0;
            glGetUniformiv(shaderProgramStereo->programId(), uniformLoc, &stereoFlagValue);
            logger->info("StereoOpenGLCommon::setStereoShaderUniforms: iStereoFlag = {} (m_stereoFormat = {}, STEREO_FORMAT_3D would be 1)",
                stereoFlagValue, static_cast<int>(m_stereoFormat));
        }
        uniformLoc = shaderProgramStereo->uniformLocation("textureY");
        if (uniformLoc < 0) {
            logger->warn("StereoOpenGLCommon::setStereoShaderUniforms: Uniform textureY not found in shader");
        } else {
            logger->debug("StereoOpenGLCommon::setStereoShaderUniforms: Uniform textureY location: {}", uniformLoc);
        }
        uniformLoc = shaderProgramStereo->uniformLocation("iVideoFormat");
        if (uniformLoc < 0) {
            logger->warn("StereoOpenGLCommon::setStereoShaderUniforms: Uniform iVideoFormat not found in shader");
        } else {
            int videoFormatValue = 0;
            glGetUniformiv(shaderProgramStereo->programId(), uniformLoc, &videoFormatValue);
            logger->info("StereoOpenGLCommon::setStereoShaderUniforms: iVideoFormat = {} (numPlanes = {})", videoFormatValue, numPlanes);
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

void StereoOpenGLCommon::updateDynamicVertices(int frameWidth, int frameHeight)
{
    // 参考 v1.0.8 FFmpegView::programDraw 的顶点计算逻辑
    // 使用交错数组格式：每个顶点5个float {x, y, z, tex_x, tex_y}
    // 顶点顺序：右上(0), 右下(1), 左下(2), 左上(3)

    // 获取当前视口尺寸
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int viewWidth = viewport[2];
    int viewHeight = viewport[3];

    if (viewWidth <= 0 || viewHeight <= 0) {
        // 视口无效，使用widget尺寸
        if (m_widget) {
            viewWidth = m_widget->width();
            viewHeight = m_widget->height();
        }
    }

    // 检查是否需要更新顶点
    bool texSizeChanged = (m_texWidth != frameWidth || m_texHeight != frameHeight);
    bool viewSizeChanged = (m_viewWidth != viewWidth || m_viewHeight != viewHeight);

    if (!texSizeChanged && !viewSizeChanged) {
        // 尺寸没有变化，检查是否需要更新视差裁剪
        if (m_parallaxShift == 0 || !m_enableStripParallaxSideView) {
            return;  // 不需要更新
        }
    }

    // 更新存储的尺寸
    m_texWidth = frameWidth;
    m_texHeight = frameHeight;
    m_viewWidth = viewWidth;
    m_viewHeight = viewHeight;

    // 计算宽高比（参考v1.0.8 FFmpegView.cc:1084-1086）
    // tr: 纹理宽高比, vr: 视口宽高比
    float tr = (float)frameWidth / frameHeight;
    float vr = (float)viewWidth / viewHeight;
    float r = tr / vr;

    if (logger) {
        static int updateCounter = 0;
        updateCounter++;
        if (updateCounter % 100 == 0) {
            logger->debug("StereoOpenGLCommon::updateDynamicVertices: frame={}x{}, view={}x{}, tr={}, vr={}, r={}",
                frameWidth, frameHeight, viewWidth, viewHeight, tr, vr, r);
        }
    }

    // 交错数组格式：每个顶点5个float {x, y, z, tex_x, tex_y}
    // 纹理坐标固定：右上(1,0), 右下(1,1), 左下(0,1), 左上(0,0)
    // 顶点顺序：右上(0), 右下(1), 左下(2), 左上(3)

    if (m_fullscreenPlusStretch) {
        // 全屏Plus模式：拉伸显示（参考v1.0.8 FFmpegView.cc:1176-1178）
        // clang-format off
        float vertices[20] = {
            1.0f,  1.0f, 0.0f,     1.0f, 0.0f,  // 右上: 位置(1,1,0), 纹理(1,0)
            1.0f, -1.0f, 0.0f,     1.0f, 1.0f,  // 右下: 位置(1,-1,0), 纹理(1,1)
           -1.0f, -1.0f, 0.0f,     0.0f, 1.0f,  // 左下: 位置(-1,-1,0), 纹理(0,1)
           -1.0f,  1.0f, 0.0f,     0.0f, 0.0f,  // 左上: 位置(-1,1,0), 纹理(0,0)
        };
        // clang-format on
        memcpy(m_vertices, vertices, sizeof(vertices));
    }
    else if (tr > vr) {
        // 纹理比视口"更宽"，上下留黑（参考v1.0.8 FFmpegView.cc:1097-1106）
        float p = vr / tr;

        // 视差调节时的裁剪效果
        float sideViewStrip = 0.0f;
        if (m_parallaxShift != 0 && m_enableStripParallaxSideView) {
            float shiftOffset = 2.0f * std::abs(m_parallaxShift) + 2.0f;
            sideViewStrip = shiftOffset / frameWidth;
        }

        // clang-format off
        float vertices[20] = {
            1.0f + sideViewStrip,  p, 0.0f,     1.0f, 0.0f,  // 右上
            1.0f + sideViewStrip, -p, 0.0f,     1.0f, 1.0f,  // 右下
           -1.0f - sideViewStrip, -p, 0.0f,     0.0f, 1.0f,  // 左下
           -1.0f - sideViewStrip,  p, 0.0f,     0.0f, 0.0f,  // 左上
        };
        // clang-format on
        memcpy(m_vertices, vertices, sizeof(vertices));
    }
    else if (tr < vr) {
        // 纹理比视口"更高"，左右留黑（参考v1.0.8 FFmpegView.cc:1121-1128）
        float sideViewStrip = 0.0f;
        if (m_parallaxShift != 0 && m_enableStripParallaxSideView) {
            float shiftOffset = 2.0f * std::abs(m_parallaxShift) + 2.0f;
            sideViewStrip = shiftOffset / frameWidth;
        }

        // clang-format off
        float vertices[20] = {
             r + sideViewStrip,  1.0f, 0.0f,     1.0f, 0.0f,  // 右上
             r + sideViewStrip, -1.0f, 0.0f,     1.0f, 1.0f,  // 右下
            -r - sideViewStrip, -1.0f, 0.0f,     0.0f, 1.0f,  // 左下
            -r - sideViewStrip,  1.0f, 0.0f,     0.0f, 0.0f,  // 左上
        };
        // clang-format on
        memcpy(m_vertices, vertices, sizeof(vertices));
    }
    else {
        // 宽高比相同，全屏显示（参考v1.0.8 FFmpegView.cc:1144-1150）
        float sideViewStrip = 0.0f;
        if (m_parallaxShift != 0 && m_enableStripParallaxSideView) {
            float shiftOffset = 2.0f * std::abs(m_parallaxShift) + 2.0f;
            sideViewStrip = shiftOffset / frameWidth;
        }

        // clang-format off
        float vertices[20] = {
            1.0f + sideViewStrip,  1.0f, 0.0f,     1.0f, 0.0f,  // 右上
            1.0f + sideViewStrip, -1.0f, 0.0f,     1.0f, 1.0f,  // 右下
           -1.0f - sideViewStrip, -1.0f, 0.0f,     0.0f, 1.0f,  // 左下
           -1.0f - sideViewStrip,  1.0f, 0.0f,     0.0f, 0.0f,  // 左上
        };
        // clang-format on
        memcpy(m_vertices, vertices, sizeof(vertices));
    }

    // 更新 VBO（参考v1.0.8 FFmpegView.cc:1169-1170）
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(m_vertices), m_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 更新宽高比
    m_ratio = r;
}
