#pragma once

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#endif

#include <QString>

class CrashHandler
{
public:
    static void initialize(const QString& dumpDir = QString());
    static void cleanup();

private:
    static QString dumpDirectory_;
    static bool initialized_;

#ifdef Q_OS_WIN
    static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo);
    static bool generateMiniDump(EXCEPTION_POINTERS* exceptionInfo, const QString& dumpPath);
    static QString getDumpFilePath();
#else
    static void signalHandler(int sig);
#endif
};
