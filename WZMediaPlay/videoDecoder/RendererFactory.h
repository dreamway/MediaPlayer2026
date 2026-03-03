#pragma once

#include "VideoRenderer.h"
#include "GlobalDef.h"
#include <memory>

/**
 * RendererFactory: 渲染器工厂类
 *
 * 职责：
 * - 创建不同类型的渲染器
 * - 简化渲染器的创建过程
 * - 提供统一的创建接口
 */
class RendererFactory
{
public:
    /**
     * 创建 OpenGL 2D 渲染器
     * @return OpenGL 2D 渲染器
     */
    static VideoRendererPtr createOpenGLRenderer();

    /**
     * 创建 OpenGL 3D 立体渲染器
     * @return OpenGL 3D 立体渲染器
     */
    static VideoRendererPtr createStereoRenderer();

    /**
     * 根据格式自动选择渲染器
     * @param format 立体格式
     * @return 适合的渲染器
     */
    static VideoRendererPtr createRenderer(StereoFormat format);
};
