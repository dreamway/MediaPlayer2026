/*
    ShaderManager: Shader 管理器实现
*/

#include "ShaderManager.h"
#include "spdlog/spdlog.h"
#include <QFile>
#include <QTextStream>
#include <QIODevice>

extern spdlog::logger *logger;

ShaderManager::ShaderManager(QObject *parent)
    : QObject(parent)
{
}

ShaderManager::~ShaderManager()
{
}

bool ShaderManager::loadInternalShaders()
{
    QFile vertexFile(":/MainWindow/Shader/vertex.glsl");
    if (!vertexFile.exists()) {
        if (logger) logger->warn("ShaderManager: Internal vertex file not exists");
        return false;
    }
    
    if (!vertexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (logger) logger->warn("ShaderManager: Failed to open internal vertex file");
        return false;
    }
    
    QTextStream vertexIn(&vertexFile);
    vertexSource_.clear();
    vertexSource_ = vertexIn.readAll();
    
    if (logger) logger->info("ShaderManager: Loaded internal vertex shader");

    QFile fragmentFile(":/MainWindow/Shader/fragment.glsl");
    if (!fragmentFile.exists()) {
        if (logger) logger->warn("ShaderManager: Internal fragment file not exists");
        return false;
    }
    
    if (!fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (logger) logger->warn("ShaderManager: Failed to open internal fragment file");
        return false;
    }
    
    QTextStream fragIn(&fragmentFile);
    fragmentSource_.clear();
    fragmentSource_ = fragIn.readAll();
    
    if (logger) logger->info("ShaderManager: Loaded internal fragment shader");
    
    return true;
}

bool ShaderManager::loadExternalShaders()
{
    QFile vertexFile("./Shader/vertex.glsl");
    if (!vertexFile.exists()) {
        if (logger) logger->warn("ShaderManager: External vertex file not exists");
        return false;
    }
    
    if (!vertexFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (logger) logger->warn("ShaderManager: Failed to open external vertex file");
        return false;
    }
    
    QTextStream vertexIn(&vertexFile);
    externalVertexSource_.clear();
    externalVertexSource_ = vertexIn.readAll();
    
    if (logger) logger->info("ShaderManager: Loaded external vertex shader");

    QFile fragmentFile("./Shader/fragment.glsl");
    if (!fragmentFile.exists()) {
        if (logger) logger->warn("ShaderManager: External fragment file not exists");
        return false;
    }
    
    if (!fragmentFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (logger) logger->warn("ShaderManager: Failed to open external fragment file");
        return false;
    }
    
    QTextStream fragIn(&fragmentFile);
    externalFragmentSource_.clear();
    externalFragmentSource_ = fragIn.readAll();
    
    if (logger) logger->info("ShaderManager: Loaded external fragment shader");
    
    // 如果外部 Shader 加载成功，替换内部 Shader
    if (!externalVertexSource_.isEmpty() && !externalFragmentSource_.isEmpty()) {
        vertexSource_ = externalVertexSource_;
        fragmentSource_ = externalFragmentSource_;
        if (logger) logger->info("ShaderManager: External shaders override internal shaders");
    }
    
    return true;
}
