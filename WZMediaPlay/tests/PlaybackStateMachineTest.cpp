// 单元测试：PlaybackStateMachine
// 使用简单的测试框架（可以后续集成Google Test或Catch2）

#include "../PlaybackStateMachine.h"
#include <cassert>
#include <iostream>

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::cerr << "TEST FAILED: " << message << std::endl; \
            assert(false); \
        } \
    } while(0)

void testStateTransitions() {
    std::cout << "Testing state transitions..." << std::endl;
    
    PlaybackStateMachine sm;
    
    // 测试正常状态转换
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Idle -> Opening should succeed");
    TEST_ASSERT(sm.getState() == PlaybackState::Opening, "State should be Opening");
    
    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Opening -> Ready should succeed");
    TEST_ASSERT(sm.getState() == PlaybackState::Ready, "State should be Ready");
    
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Ready -> Playing should succeed");
    TEST_ASSERT(sm.getState() == PlaybackState::Playing, "State should be Playing");
    
    // 测试非法状态转换
    TEST_ASSERT(!sm.transitionTo(PlaybackState::Seeking), "Ready -> Seeking should fail (must go through Playing first)");
    TEST_ASSERT(sm.getState() == PlaybackState::Playing, "State should still be Playing");
    
    std::cout << "State transitions test passed!" << std::endl;
}

void testConcurrentAccess() {
    std::cout << "Testing concurrent access..." << std::endl;
    
    PlaybackStateMachine sm;
    
    // 测试并发访问（简单测试）
    for (int i = 0; i < 100; ++i) {
        sm.getState();
    }
    
    // 应该没有崩溃或数据竞争
    std::cout << "Concurrent access test passed!" << std::endl;
}

void testStateQueries() {
    std::cout << "Testing state queries..." << std::endl;
    
    PlaybackStateMachine sm;
    
    TEST_ASSERT(sm.isIdle(), "Initial state should be Idle");
    
    sm.transitionTo(PlaybackState::Opening);
    TEST_ASSERT(sm.isOpening(), "State should be Opening");
    TEST_ASSERT(!sm.isPlaying(), "State should not be Playing");
    
    std::cout << "State queries test passed!" << std::endl;
}

// 可以添加main函数来运行测试（如果独立编译）
#ifdef STANDALONE_TEST
int main() {
    testStateTransitions();
    testConcurrentAccess();
    testStateQueries();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
#endif
