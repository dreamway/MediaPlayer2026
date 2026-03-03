#pragma once

#include <QString>

/**
 * CrashHandler: 跨平台崩溃捕获
 * - Windows: SetUnhandledExceptionFilter + MiniDump
 * - Linux/macOS: signal handler (SIGSEGV/SIGABRT/SIGFPE)
 */
class CrashHandler
{
public:
    static void initialize(const QString& dumpDir = QString());
    static void cleanup();

private:
    static QString dumpDirectory_;
    static bool initialized_;

#ifdef WIN32
#include <windows.h>
#include <dbghelp.h>
    static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
    static bool generateMiniDump(EXCEPTION_POINTERS* exceptionInfo, const QString& dumpPath);
    static QString getDumpFilePath();
#else
    static void signalHandler(int sig);
#endif
};

