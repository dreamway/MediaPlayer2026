#include "ErrorRecoveryManager.h"
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

ErrorRecoveryManager::ErrorRecoveryManager() {
}

RecoveryResult ErrorRecoveryManager::handleError(const ErrorInfo& error) {
    std::lock_guard<std::mutex> lock(errorCountMutex_);
    
    // 更新错误计数
    int retryCount = 0;
    switch (error.type) {
        case ErrorType::DecodeError:
            decodeErrorCount_++;
            retryCount = decodeErrorCount_;
            break;
        case ErrorType::NetworkError:
            networkErrorCount_++;
            retryCount = networkErrorCount_;
            break;
        case ErrorType::ResourceError:
            resourceErrorCount_++;
            retryCount = resourceErrorCount_;
            break;
        case ErrorType::ThreadError:
            threadErrorCount_++;
            retryCount = threadErrorCount_;
            break;
        case ErrorType::UnknownError:
            unknownErrorCount_++;
            retryCount = unknownErrorCount_;
            break;
    }
    
    // 获取恢复动作
    RecoveryAction action = getRecoveryAction(error.type, retryCount);
    bool shouldContinue = (action != RecoveryAction::StopPlayback);
    
    if (logger) {
        logger->warn("ErrorRecoveryManager: Error {} (type: {}, retry: {}), action: {}",
                    error.message, static_cast<int>(error.type), retryCount, static_cast<int>(action));
    }
    
    RecoveryResult result(action, retryCount, shouldContinue);
    
    // 调用回调
    if (errorCallback_) {
        errorCallback_(error, result);
    }
    
    return result;
}

void ErrorRecoveryManager::resetErrorCount(ErrorType type) {
    std::lock_guard<std::mutex> lock(errorCountMutex_);
    switch (type) {
        case ErrorType::DecodeError:
            decodeErrorCount_ = 0;
            break;
        case ErrorType::NetworkError:
            networkErrorCount_ = 0;
            break;
        case ErrorType::ResourceError:
            resourceErrorCount_ = 0;
            break;
        case ErrorType::ThreadError:
            threadErrorCount_ = 0;
            break;
        case ErrorType::UnknownError:
            unknownErrorCount_ = 0;
            break;
    }
}

int ErrorRecoveryManager::getErrorCount(ErrorType type) const {
    std::lock_guard<std::mutex> lock(errorCountMutex_);
    switch (type) {
        case ErrorType::DecodeError:
            return decodeErrorCount_;
        case ErrorType::NetworkError:
            return networkErrorCount_;
        case ErrorType::ResourceError:
            return resourceErrorCount_;
        case ErrorType::ThreadError:
            return threadErrorCount_;
        case ErrorType::UnknownError:
            return unknownErrorCount_;
        default:
            return 0;
    }
}

int ErrorRecoveryManager::getTotalErrorCount() const {
    std::lock_guard<std::mutex> lock(errorCountMutex_);
    return decodeErrorCount_ + networkErrorCount_ + resourceErrorCount_ + 
           threadErrorCount_ + unknownErrorCount_;
}

void ErrorRecoveryManager::setMaxRetries(ErrorType type, int maxRetries) {
    std::lock_guard<std::mutex> lock(errorCountMutex_);
    switch (type) {
        case ErrorType::DecodeError:
            maxDecodeRetries_ = maxRetries;
            break;
        case ErrorType::NetworkError:
            maxNetworkRetries_ = maxRetries;
            break;
        case ErrorType::ResourceError:
            maxResourceRetries_ = maxRetries;
            break;
        case ErrorType::ThreadError:
            maxThreadRetries_ = maxRetries;
            break;
        case ErrorType::UnknownError:
            maxUnknownRetries_ = maxRetries;
            break;
    }
}

void ErrorRecoveryManager::setErrorCallback(
    std::function<void(const ErrorInfo&, const RecoveryResult&)> callback) {
    errorCallback_ = callback;
}

RecoveryAction ErrorRecoveryManager::getRecoveryAction(ErrorType type, int retryCount) const {
    int maxRetries = 0;
    switch (type) {
        case ErrorType::DecodeError:
            maxRetries = maxDecodeRetries_;
            if (retryCount <= maxRetries) {
                return RecoveryAction::FlushAndRetry;
            }
            break;
        case ErrorType::NetworkError:
            maxRetries = maxNetworkRetries_;
            if (retryCount <= maxRetries) {
                return RecoveryAction::Retry;
            }
            break;
        case ErrorType::ResourceError:
            maxRetries = maxResourceRetries_;
            if (retryCount <= maxRetries) {
                return RecoveryAction::Retry;
            }
            break;
        case ErrorType::ThreadError:
            maxRetries = maxThreadRetries_;
            if (retryCount <= maxRetries) {
                return RecoveryAction::RestartThread;
            }
            break;
        case ErrorType::UnknownError:
            maxRetries = maxUnknownRetries_;
            if (retryCount <= maxRetries) {
                return RecoveryAction::Retry;
            }
            break;
    }
    
    // 超过最大重试次数，停止播放
    return RecoveryAction::StopPlayback;
}
