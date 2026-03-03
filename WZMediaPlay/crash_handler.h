#pragma once

#ifdef WIN32
#include <windows.h>
#include <dbghelp.h>
#include <QString>

/**
 * 崩溃转储捕获和处理
 * 在Windows上使用SetUnhandledExceptionFilter捕获未处理的异常
 * 并生成minidump文件用于后续分析
 */
class CrashHandler
{
public:
    /**
     * 初始化崩溃处理器
     * 应该在main函数开始时调用
     * @param dumpDir 转储文件保存目录（如果为空，使用当前目录）
     */
    static void initialize(const QString& dumpDir = QString());
    
    /**
     * 清理资源
     */
    static void cleanup();
    
private:
    /**
     * 未处理异常过滤器
     * 这是Windows异常处理的核心回调函数
     */
    static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
    
    /**
     * 生成minidump文件
     */
    static bool generateMiniDump(EXCEPTION_POINTERS* exceptionInfo, const QString& dumpPath);
    
    /**
     * 获取转储文件路径
     */
    static QString getDumpFilePath();
    
    static QString dumpDirectory_;
    static bool initialized_;
};

#endif // WIN32

