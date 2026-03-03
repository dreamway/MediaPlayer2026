#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

/**
 * ErrorRecoveryManager: 错误恢复管理器
 * 
 * 统一管理错误处理和自动恢复机制
 */
enum class ErrorType {
    DecodeError,        // 解码错误
    NetworkError,       // 网络错误
    ResourceError,      // 资源错误
    ThreadError,        // 线程错误
    UnknownError        // 未知错误
};

enum class RecoveryAction {
    None,               // 不处理
    Retry,              // 重试
    FlushAndRetry,      // 刷新并重试
    RestartThread,      // 重启线程
    StopPlayback        // 停止播放
};

struct ErrorInfo {
    ErrorType type;
    std::string message;
    int errorCode;
    std::chrono::steady_clock::time_point timestamp;
    
    ErrorInfo(ErrorType t, const std::string& msg, int code = 0)
        : type(t), message(msg), errorCode(code), timestamp(std::chrono::steady_clock::now()) {}
};

struct RecoveryResult {
    RecoveryAction action;
    int retryCount;
    bool shouldContinue;
    
    RecoveryResult(RecoveryAction a, int count, bool cont)
        : action(a), retryCount(count), shouldContinue(cont) {}
};

class ErrorRecoveryManager {
public:
    ErrorRecoveryManager();
    ~ErrorRecoveryManager() = default;

    // 处理错误，返回恢复动作
    RecoveryResult handleError(const ErrorInfo& error);
    
    // 重置错误计数
    void resetErrorCount(ErrorType type);
    
    // 获取错误统计
    int getErrorCount(ErrorType type) const;
    int getTotalErrorCount() const;
    
    // 设置最大重试次数
    void setMaxRetries(ErrorType type, int maxRetries);
    
    // 设置错误回调（用于外部处理）
    void setErrorCallback(std::function<void(const ErrorInfo&, const RecoveryResult&)> callback);

private:
    // 获取恢复动作
    RecoveryAction getRecoveryAction(ErrorType type, int retryCount) const;
    
    // 错误计数
    mutable std::mutex errorCountMutex_;
    int decodeErrorCount_{0};
    int networkErrorCount_{0};
    int resourceErrorCount_{0};
    int threadErrorCount_{0};
    int unknownErrorCount_{0};
    
    // 最大重试次数
    int maxDecodeRetries_{3};
    int maxNetworkRetries_{3};
    int maxResourceRetries_{1};
    int maxThreadRetries_{1};
    int maxUnknownRetries_{1};
    
    // 错误回调
    std::function<void(const ErrorInfo&, const RecoveryResult&)> errorCallback_;
};
