/*
    ShaderManager: Shader 管理器（从 FFmpegView 分离）
    职责：管理 Shader 的加载和初始化
*/

#pragma once

#include <QString>
#include <QObject>

/**
 * ShaderManager: Shader 管理器
 * 
 * 职责：
 * - 加载内部 Shader（从资源文件）
 * - 加载外部 Shader（从文件系统）
 * - 提供 Shader 源码访问接口
 */
class ShaderManager : public QObject
{
    Q_OBJECT

public:
    explicit ShaderManager(QObject *parent = nullptr);
    ~ShaderManager();

    // 加载内部 Shader（从资源文件）
    bool loadInternalShaders();

    // 加载外部 Shader（从文件系统，优先级高于内部 Shader）
    bool loadExternalShaders();

    // 获取 Vertex Shader 源码
    QString getVertexSource() const { return vertexSource_; }

    // 获取 Fragment Shader 源码
    QString getFragmentSource() const { return fragmentSource_; }

    // 检查 Shader 是否已加载
    bool isShaderLoaded() const { return !vertexSource_.isEmpty() && !fragmentSource_.isEmpty(); }

private:
    QString vertexSource_;
    QString fragmentSource_;
    
    QString externalVertexSource_;
    QString externalFragmentSource_;
};
