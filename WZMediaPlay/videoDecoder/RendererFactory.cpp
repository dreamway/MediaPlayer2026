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

    switch (format) {
        case STEREO_FORMAT_NORMAL_2D:
            renderer = createOpenGLRenderer();
            break;

        case STEREO_FORMAT_3D:
            renderer = createStereoRenderer();
            break;

        default:
            // 默认使用 2D 渲染器
            renderer = createOpenGLRenderer();
            break;
    }

    if (renderer && logger) {
        logger->info("RendererFactory::createRenderer: Created renderer for format {}",
                     static_cast<int>(format));
    }

    return renderer;
}
