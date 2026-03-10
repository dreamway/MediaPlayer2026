/*
    CameraRenderer: Camera 渲染器实现
*/

#include "CameraRenderer.h"
#include "GlobalDef.h"
#include "videoDecoder/opengl/DebugImageSaver.h"
#include "spdlog/spdlog.h"
#include <QDir>
#include <QDateTime>
#include <vector>

extern spdlog::logger *logger;

// 简单的 vertex shader（与 FFmpegView 兼容）
static const char* cameraVertexShader = R"(
#version 150 core
in vec3 aPos;
in vec2 aTexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position = vec4(aPos, 1.0);
    TexCoord = aTexCoord;
}
)";

// 简单的 fragment shader（用于 Camera 渲染）
// 注意：如果使用 GL_BGRA 格式上传纹理，需要在 Shader 中转换回 RGBA
static const char* cameraFragmentShader = R"(
#version 150 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D cameraTexture;

void main()
{
    vec4 texColor = texture(cameraTexture, TexCoord);
    // 如果纹理是 BGRA 格式，转换为 RGBA（在小端系统上，QImage::Format_ARGB32 的内存布局是 BGRA）
    // 但 OpenGL 会自动处理，所以这里直接使用即可
    FragColor = texColor;
}
)";

// 顶点数据（全屏四边形）
static float vertices[] = {
    // 位置          // 纹理坐标
    -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
     1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
     1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
    -1.0f,  1.0f, 0.0f,  0.0f, 0.0f
};

static unsigned int indices[] = {
    0, 1, 2,
    2, 3, 0
};

CameraRenderer::CameraRenderer(QObject *parent)
    : QObject(parent)
    , mShaderProgram(nullptr)
    , mCameraTexture(nullptr)
    , VAO(0)
    , VBO(0)
    , EBO(0)
    , mInitialized(false)
    , mCameraTextureId(0)
    , texWidth(0)
    , texHeight(0)
    , viewWidth(0)
    , viewHeight(0)
    , ratio(-1.0f)
    , debugImageSaver_(nullptr)
    , frameSaveCounter_(0)
{
    // 延迟创建 DebugImageSaver，避免在构造函数中立即启动线程导致启动卡死
    // 只在需要保存图片时才创建（在 onCameraFrameChanged 中）
}

CameraRenderer::~CameraRenderer()
{
    cleanupGL();
}

bool CameraRenderer::initializeGL()
{
    if (mInitialized) {
        return true;
    }

    initializeOpenGLFunctions();

    // 创建 Shader 程序
    if (!initializeShader()) {
        if (logger) logger->error("CameraRenderer: Failed to initialize shader");
        return false;
    }

    // 创建 VAO, VBO, EBO
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 位置属性
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 纹理坐标属性
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // 创建纹理（使用原生 OpenGL，参考原实现）
    glGenTextures(1, &mCameraTextureId);
    glBindTexture(GL_TEXTURE_2D, mCameraTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // QOpenGLTexture 不再需要（使用原生 OpenGL 纹理）
    mCameraTexture = nullptr;

    mInitialized = true;
    
    if (logger) logger->info("CameraRenderer: Initialized successfully");
    return true;
}

bool CameraRenderer::initializeShader()
{
    mShaderProgram = new QOpenGLShaderProgram(this);
    
    if (!mShaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, cameraVertexShader)) {
        if (logger) logger->error("CameraRenderer: Failed to compile vertex shader: {}", 
                                  mShaderProgram->log().toStdString());
        return false;
    }
    
    if (!mShaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, cameraFragmentShader)) {
        if (logger) logger->error("CameraRenderer: Failed to compile fragment shader: {}", 
                                  mShaderProgram->log().toStdString());
        return false;
    }
    
    if (!mShaderProgram->link()) {
        if (logger) logger->error("CameraRenderer: Failed to link shader program: {}", 
                                  mShaderProgram->log().toStdString());
        return false;
    }
    
    return true;
}

void CameraRenderer::renderCameraFrame(int width, int height)
{
    if (!mInitialized) {
        if (logger) {
            static int skipCount = 0;
            if (skipCount++ % 100 == 0) {
                logger->warn("CameraRenderer::renderCameraFrame: Not initialized");
            }
        }
        return;
    }
    
    if (!mShaderProgram) {
        if (logger) {
            logger->error("CameraRenderer::renderCameraFrame: Shader program is null");
        }
        return;
    }
    
    if (mCameraImage.isNull()) {
        if (logger) {
            static int skipCount = 0;
            if (skipCount++ % 100 == 0) {
                logger->debug("CameraRenderer::renderCameraFrame: Camera image is null (count: {})", skipCount);
            }
        }
        return;
    }
    
    if (mCameraTextureId == 0) {
        if (logger) {
            logger->error("CameraRenderer::renderCameraFrame: Texture ID is 0");
        }
        return;
    }

    // 更新视口尺寸
    viewWidth = width;
    viewHeight = height;

    // 更新纹理（使用 Camera 图像的实际尺寸）
    int imgWidth = mCameraImage.width();
    int imgHeight = mCameraImage.height();
    
    if (imgWidth <= 0 || imgHeight <= 0) {
        if (logger) {
            logger->warn("CameraRenderer::renderCameraFrame: Invalid image size: {}x{}", imgWidth, imgHeight);
        }
        return;
    }

    // 检查纹理尺寸是否改变
    bool textureSizeChanged = false;
    if (texWidth != imgWidth) {
        texWidth = imgWidth;
        textureSizeChanged = true;
    }
    if (texHeight != imgHeight) {
        texHeight = imgHeight;
        textureSizeChanged = true;
    }

    // 计算宽高比（参考原实现）
    float tr = (float)texWidth / texHeight;
    float vr = (float)viewWidth / viewHeight;
    float r = tr / vr;

    // 如果宽高比改变，更新顶点数据（参考原实现）
    if (r != ratio || textureSizeChanged) {
        ratio = r;
        
        std::vector<float> vertices;
        if (tr > vr) {
            // 上下边界不渲染，保持原背景效果
            float p = vr / tr;
            vertices = {
                 1.0f,  p, 0.0f,     1.0f, 0.0f,
                 1.0f, -p, 0.0f,     1.0f, 1.0f,
                -1.0f, -p, 0.0f,     0.0f, 1.0f,
                -1.0f,  p, 0.0f,     0.0f, 0.0f,
            };
        } else if (tr < vr) {
            // 左右两侧裁剪
            vertices = {
                 r,  1.0f, 0.0f,     1.0f, 0.0f,
                 r, -1.0f, 0.0f,     1.0f, 1.0f,
                -r, -1.0f, 0.0f,     0.0f, 1.0f,
                -r,  1.0f, 0.0f,     0.0f, 0.0f,
            };
        } else {
            // 拉伸显示，全屏
            vertices = {
                 1.0f,  1.0f, 0.0f,     1.0f, 0.0f,
                 1.0f, -1.0f, 0.0f,     1.0f, 1.0f,
                -1.0f, -1.0f, 0.0f,     0.0f, 1.0f,
                -1.0f,  1.0f, 0.0f,     0.0f, 0.0f,
            };
        }

        // 更新 VBO 数据
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(float), vertices.data());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        if (logger && textureSizeChanged) {
            logger->info("CameraRenderer::renderCameraFrame: Texture size changed to {}x{}, ratio: {}", 
                        texWidth, texHeight, r);
        }
    }

    // 绑定纹理并上传数据（使用 glTexImage2D，参考原实现）
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mCameraTextureId);
    
    // 使用 glTexImage2D 上传纹理数据（参考原实现）
    // 注意：QImage::Format_ARGB32 的内存布局是 BGRA（在小端系统上），但 OpenGL 期望 RGBA
    // 需要转换格式或使用 GL_BGRA
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    
    // 如果图像格式是 ARGB32 或 RGB32，在小端系统上内存布局是 BGRA
    // 使用 GL_BGRA 可以避免格式转换（更高效）
    if (mCameraImage.format() == QImage::Format_ARGB32 || mCameraImage.format() == QImage::Format_RGB32) {
        format = GL_BGRA;  // 使用 BGRA 格式，避免数据转换
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imgWidth, imgHeight, 0, format, type, mCameraImage.constBits());
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    // 使用 Shader 渲染
    mShaderProgram->bind();
    
    // 设置纹理 uniform（确保纹理单元 0 已绑定）
    mShaderProgram->setUniformValue("cameraTexture", 0);

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    mShaderProgram->release();
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    if (logger) {
        static int renderCount = 0;
        if (renderCount++ % 300 == 0) {
            logger->debug("CameraRenderer::renderCameraFrame: Rendered frame {} (image: {}x{}, viewport: {}x{})", 
                         renderCount, imgWidth, imgHeight, width, height);
        }
    }
    
    // frameSaveCounter_ 在 onCameraFrameChanged 中递增
}

void CameraRenderer::cleanupGL()
{
#if ENABLE_VIDEO_TRACE
    if (debugImageSaver_) {
        debugImageSaver_->stop();
        delete debugImageSaver_;
        debugImageSaver_ = nullptr;
    }
#endif
    
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    
    if (mCameraTextureId) {
        glDeleteTextures(1, &mCameraTextureId);
        mCameraTextureId = 0;
    }
    
    if (mCameraTexture) {
        delete mCameraTexture;
        mCameraTexture = nullptr;
    }
    
    if (mShaderProgram) {
        delete mShaderProgram;
        mShaderProgram = nullptr;
    }
    
    mInitialized = false;
}

void CameraRenderer::onCameraFrameChanged(const QVideoFrame &frame)
{
    QVideoFrame videoFrame = frame;
    
    if (frame.isValid()) {
        mCameraImage = frame.toImage();
        
        // 确保图像格式为 RGBA（转换为 Format_RGBA8888 或 Format_ARGB32）
        if (!mCameraImage.isNull()) {
            // 如果格式不是 ARGB32 或 RGB32，转换为 ARGB32（与 OpenGL RGBA 兼容）
            if (mCameraImage.format() != QImage::Format_ARGB32 && 
                mCameraImage.format() != QImage::Format_RGB32) {
                mCameraImage = mCameraImage.convertToFormat(QImage::Format_ARGB32);
            }
        }
        
        if (logger) {
            static int frameCount = 0;
            frameCount++;
            if (frameCount % 100 == 0) {
                QString formatStr = mCameraImage.format() == QImage::Format_RGB32 ? "RGB32" : 
                                   mCameraImage.format() == QImage::Format_ARGB32 ? "ARGB32" : 
                                   QString::number(mCameraImage.format());
                logger->info("CameraRenderer::onCameraFrameChanged: Received frame {} (image size: {}x{}, format: {})", 
                            frameCount, mCameraImage.width(), mCameraImage.height(), formatStr.toStdString());
            }
        }
        
        // 递增帧计数器
        frameSaveCounter_++;
        
#if ENABLE_VIDEO_TRACE
        // 保存接收到的 QVideoFrame（转换为 QImage 后保存）
        // 延迟创建 DebugImageSaver，避免在构造函数中立即启动线程
        if (!debugImageSaver_ && !mCameraImage.isNull()) {
            debugImageSaver_ = new DebugImageSaver(this);
            if (logger) {
                logger->info("CameraRenderer: DebugImageSaver created lazily for QVideoFrame saving");
            }
        }

        if (debugImageSaver_ && !mCameraImage.isNull()) {
            // 每10帧保存一次，避免保存过多图片
            if (frameSaveCounter_ % 10 == 0) {
                QDateTime now = QDateTime::currentDateTime();
                QString timeStr = now.toString("yyyyMMddHHmmsszzz");
                QString filename = QString("debug_frames_camera/camera_frame_%1_%2_%3x%4.png")
                    .arg(timeStr)
                    .arg(frameSaveCounter_)
                    .arg(mCameraImage.width())
                    .arg(mCameraImage.height());
                debugImageSaver_->enqueueImage(mCameraImage, filename);

                if (logger && frameSaveCounter_ % 100 == 0) {
                    logger->info("CameraRenderer::onCameraFrameChanged: Saved frame {} to {}",
                                frameSaveCounter_, filename.toStdString());
                }
            }
        }
#endif
        
        emit frameUpdated();
    } else {
        if (logger) {
            static int invalidFrameCount = 0;
            if (invalidFrameCount++ % 100 == 0) {
                logger->warn("CameraRenderer::onCameraFrameChanged: Received invalid frame (count: {})", invalidFrameCount);
            }
        }
    }
}
