// 单元测试：ErrorRecoveryManager

#include "../ErrorRecoveryManager.h"
#include <cassert>
#include <iostream>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "TEST FAILED: " << message << std::endl; \
            assert(false); \
        } \
    } while(0)

void testDecodeErrorRecovery() {
    std::cout << "Testing decode error recovery..." << std::endl;
    
    ErrorRecoveryManager recovery;
    recovery.setMaxRetries(ErrorType::DecodeError, 3);
    
    // 模拟解码错误
    ErrorInfo error(ErrorType::DecodeError, "Decoder returned EOF");
    
    // 第一次错误应该flush并重试
    auto result1 = recovery.handleError(error);
    TEST_ASSERT(result1.action == RecoveryAction::FlushAndRetry, "First decode error should flush and retry");
    TEST_ASSERT(result1.retryCount == 1, "Retry count should be 1");
    TEST_ASSERT(result1.shouldContinue, "Should continue after first error");
    
    // 第二次错误
    auto result2 = recovery.handleError(error);
    TEST_ASSERT(result2.retryCount == 2, "Retry count should be 2");
    
    // 第三次错误
    auto result3 = recovery.handleError(error);
    TEST_ASSERT(result3.retryCount == 3, "Retry count should be 3");
    
    // 第四次错误应该停止播放
    auto result4 = recovery.handleError(error);
    TEST_ASSERT(result4.action == RecoveryAction::StopPlayback, "Fourth error should stop playback");
    TEST_ASSERT(!result4.shouldContinue, "Should not continue after max retries");
    
    std::cout << "Decode error recovery test passed!" << std::endl;
}

void testErrorCountReset() {
    std::cout << "Testing error count reset..." << std::endl;
    
    ErrorRecoveryManager recovery;
    
    ErrorInfo error(ErrorType::DecodeError, "Test error");
    recovery.handleError(error);
    recovery.handleError(error);
    
    TEST_ASSERT(recovery.getErrorCount(ErrorType::DecodeError) == 2, "Error count should be 2");
    
    recovery.resetErrorCount(ErrorType::DecodeError);
    TEST_ASSERT(recovery.getErrorCount(ErrorType::DecodeError) == 0, "Error count should be 0 after reset");
    
    std::cout << "Error count reset test passed!" << std::endl;
}

#ifdef STANDALONE_TEST
int main() {
    testDecodeErrorRecovery();
    testErrorCountReset();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
#endif
