#include "crash_handler.h"
#include "spdlog/spdlog.h"
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QCoreApplication>

#ifdef Q_OS_WIN

QString CrashHandler::dumpDirectory_;
bool CrashHandler::initialized_ = false;

void CrashHandler::initialize(const QString& dumpDir)
{
    if (initialized_) {
        return;
    }
    
    // 设置转储目录
    if (dumpDir.isEmpty()) {
        // 默认使用应用程序目录下的crash_dumps文件夹
        QString appDir = QCoreApplication::applicationDirPath();
        dumpDirectory_ = QDir(appDir).filePath("crash_dumps");
    } else {
        dumpDirectory_ = dumpDir;
    }
    
    // 确保目录存在
    QDir dir;
    if (!dir.exists(dumpDirectory_)) {
        dir.mkpath(dumpDirectory_);
    }
    
    // 设置未处理异常过滤器
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
    
    initialized_ = true;
    
    auto logger = spdlog::get("logger");
    if (logger) {
        logger->info("Crash handler initialized, dump directory: {}", dumpDirectory_.toStdString());
    }
}

void CrashHandler::cleanup()
{
    if (initialized_) {
        SetUnhandledExceptionFilter(nullptr);
        initialized_ = false;
    }
}

LONG WINAPI CrashHandler::unhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo)
{
    auto logger = spdlog::get("logger");
    
    if (logger) {
        logger->critical("==========================================");
        logger->critical("CRASH DETECTED!");
        logger->critical("==========================================");
        logger->critical("Exception Code: 0x{:08X}", exceptionInfo->ExceptionRecord->ExceptionCode);
        logger->critical("Exception Address: 0x{:016X}", 
                        reinterpret_cast<uintptr_t>(exceptionInfo->ExceptionRecord->ExceptionAddress));
        
        // 记录寄存器信息
        if (exceptionInfo->ContextRecord) {
            #ifdef _WIN64
            logger->critical("RAX: 0x{:016X}", exceptionInfo->ContextRecord->Rax);
            logger->critical("RBX: 0x{:016X}", exceptionInfo->ContextRecord->Rbx);
            logger->critical("RCX: 0x{:016X}", exceptionInfo->ContextRecord->Rcx);
            logger->critical("RDX: 0x{:016X}", exceptionInfo->ContextRecord->Rdx);
            logger->critical("RIP: 0x{:016X}", exceptionInfo->ContextRecord->Rip);
            logger->critical("RSP: 0x{:016X}", exceptionInfo->ContextRecord->Rsp);
            #else
            logger->critical("EAX: 0x{:08X}", exceptionInfo->ContextRecord->Eax);
            logger->critical("EBX: 0x{:08X}", exceptionInfo->ContextRecord->Ebx);
            logger->critical("ECX: 0x{:08X}", exceptionInfo->ContextRecord->Ecx);
            logger->critical("EDX: 0x{:08X}", exceptionInfo->ContextRecord->Edx);
            logger->critical("EIP: 0x{:08X}", exceptionInfo->ContextRecord->Eip);
            logger->critical("ESP: 0x{:08X}", exceptionInfo->ContextRecord->Esp);
            #endif
        }
        
        logger->critical("==========================================");
    }
    
    // 生成minidump
    QString dumpPath = getDumpFilePath();
    if (generateMiniDump(exceptionInfo, dumpPath)) {
        if (logger) {
            logger->critical("Crash dump saved to: {}", dumpPath.toStdString());
        }
    } else {
        if (logger) {
            logger->critical("Failed to generate crash dump");
        }
    }
    
    // 刷新日志
    if (logger) {
        logger->flush();
    }
    
    // 返回EXCEPTION_EXECUTE_HANDLER会终止程序
    // 返回EXCEPTION_CONTINUE_SEARCH会让系统继续处理（显示Windows错误对话框）
    return EXCEPTION_EXECUTE_HANDLER;
}

bool CrashHandler::generateMiniDump(EXCEPTION_POINTERS* exceptionInfo, const QString& dumpPath)
{
    HANDLE hFile = CreateFileW(
        dumpPath.toStdWString().c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    MINIDUMP_EXCEPTION_INFORMATION exceptionParam;
    exceptionParam.ThreadId = GetCurrentThreadId();
    exceptionParam.ExceptionPointers = exceptionInfo;
    exceptionParam.ClientPointers = FALSE;
    
    // 生成minidump
    MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs | 
        MiniDumpWithHandleData | 
        MiniDumpWithThreadInfo
    );
    
    BOOL success = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        hFile,
        dumpType,
        exceptionInfo ? &exceptionParam : nullptr,
        nullptr,
        nullptr
    );
    
    CloseHandle(hFile);
    
    return success == TRUE;
}

QString CrashHandler::getDumpFilePath()
{
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString fileName = QString("crash_%1.dmp").arg(timestamp);
    return QDir(dumpDirectory_).filePath(fileName);
}

#else

#include <csignal>
#include <cstdlib>
#include <execinfo.h>

QString CrashHandler::dumpDirectory_;
bool CrashHandler::initialized_ = false;

void CrashHandler::initialize(const QString& dumpDir)
{
    if (initialized_) return;

    dumpDirectory_ = dumpDir.isEmpty()
        ? QCoreApplication::applicationDirPath() + "/crash_dumps"
        : dumpDir;

    QDir dir;
    if (!dir.exists(dumpDirectory_)) {
        dir.mkpath(dumpDirectory_);
    }

    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGFPE,  signalHandler);

    initialized_ = true;

    auto log = spdlog::get("logger");
    if (log) {
        log->info("CrashHandler initialized (Linux signal handler), dump dir: {}", dumpDirectory_.toStdString());
    }
}

void CrashHandler::cleanup()
{
    if (initialized_) {
        signal(SIGSEGV, SIG_DFL);
        signal(SIGABRT, SIG_DFL);
        signal(SIGFPE,  SIG_DFL);
        initialized_ = false;
    }
}

void CrashHandler::signalHandler(int sig)
{
    auto log = spdlog::get("logger");
    if (log) {
        log->critical("==========================================");
        log->critical("CRASH DETECTED! Signal: {}", sig);
        log->critical("==========================================");
    }

    void *frames[64];
    int nframes = backtrace(frames, 64);
    char **symbols = backtrace_symbols(frames, nframes);
    if (symbols && log) {
        for (int i = 0; i < nframes; ++i) {
            log->critical("  [{}] {}", i, symbols[i]);
        }
        free(symbols);
    }

    if (log) {
        log->flush();
    }

    signal(sig, SIG_DFL);
    raise(sig);
}
#endif
