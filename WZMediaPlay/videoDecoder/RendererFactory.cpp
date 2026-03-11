/*
    RendererFactory: 渲染器工厂类实现
*/

#include "RendererFactory.h"
#include "opengl/OpenGLRenderer.h"
#include "opengl/StereoOpenGLRenderer.h"
#include "GlobalDef.h"

#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

VideoRendererPtr RendererFactory::createOpenGLRenderer()
{
    auto renderer = std::make_shared<OpenGLRenderer>();

    if (logger) {
        logger->info("RendererFactory::createOpenGLRenderer: Created OpenGL 2D renderer");
    }

    return renderer;
}

VideoRendererPtr RendererFactory::createStereoRenderer()
{
    auto renderer = std::make_shared<StereoOpenGLRenderer>();

    if (logger) {
        logger->info("RendererFactory::createStereoRenderer: Created OpenGL 3D stereo renderer");
    }

    return renderer;
}

VideoRendererPtr RendererFactory::createRenderer(StereoFormat format)
{
    VideoRendererPtr renderer;

    // 始终使用 StereoOpenGLRenderer，因为它同时支持 2D 和 3D 模式
    // 这样视差调节功能在任何模式下都能工作
    // - 2D 模式：调用 OpenGLRenderer::render() 处理
    // - 3D 模式：使用 StereoOpenGLCommon 进行立体渲染
    renderer = createStereoRenderer();

    if (renderer && logger) {
        logger->info("RendererFactory::createRenderer: Created StereoOpenGLRenderer for format {} (supports both 2D and 3D)",
                     static_cast<int>(format));
    }

    return renderer;
}
