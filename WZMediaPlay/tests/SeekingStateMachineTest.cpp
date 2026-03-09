// 单元测试：PlaybackStateMachine 的 enterSeeking/exitSeeking（Seeking 状态与恢复）
// 与 PlaybackStateMachineTest 同风格，可独立编译或由 CMake 与 test_logger_stub 链接

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

static void testEnterExitSeekingFromPlaying()
{
    std::cout << "Testing enterSeeking/exitSeeking from Playing..." << std::endl;

    PlaybackStateMachine sm;
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Idle -> Opening");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Opening -> Ready");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Ready -> Playing");
    TEST_ASSERT(sm.isPlaying(), "Should be Playing");

    TEST_ASSERT(sm.enterSeeking("test seek"), "enterSeeking from Playing should succeed");
    TEST_ASSERT(sm.isSeeking(), "State should be Seeking");
    TEST_ASSERT(sm.preSeekState() == PlaybackState::Playing, "preSeekState should be Playing");

    TEST_ASSERT(sm.exitSeeking("seek done"), "exitSeeking should succeed");
    TEST_ASSERT(sm.isPlaying(), "State should restore to Playing");
    TEST_ASSERT(!sm.isSeeking(), "Should not be Seeking");

    std::cout << "enterSeeking/exitSeeking from Playing passed!" << std::endl;
}

static void testEnterExitSeekingFromPaused()
{
    std::cout << "Testing enterSeeking/exitSeeking from Paused..." << std::endl;

    PlaybackStateMachine sm;
    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);
    sm.transitionTo(PlaybackState::Paused);
    TEST_ASSERT(sm.isPaused(), "Should be Paused");

    TEST_ASSERT(sm.enterSeeking("test seek from pause"), "enterSeeking from Paused should succeed");
    TEST_ASSERT(sm.isSeeking(), "State should be Seeking");
    TEST_ASSERT(sm.preSeekState() == PlaybackState::Paused, "preSeekState should be Paused");

    TEST_ASSERT(sm.exitSeeking("seek done"), "exitSeeking should succeed");
    TEST_ASSERT(sm.isPaused(), "State should restore to Paused");

    std::cout << "enterSeeking/exitSeeking from Paused passed!" << std::endl;
}

static void testEnterSeekingRejectedWhenNotPlayingOrPaused()
{
    std::cout << "Testing enterSeeking rejected when not Playing/Paused..." << std::endl;

    PlaybackStateMachine sm;
    TEST_ASSERT(sm.isIdle(), "Initial state Idle");
    TEST_ASSERT(!sm.enterSeeking("from Idle"), "enterSeeking from Idle should fail");
    TEST_ASSERT(sm.isIdle(), "State should still be Idle");

    sm.transitionTo(PlaybackState::Opening);
    TEST_ASSERT(!sm.enterSeeking("from Opening"), "enterSeeking from Opening should fail");

    sm.transitionTo(PlaybackState::Ready);
    TEST_ASSERT(!sm.enterSeeking("from Ready"), "enterSeeking from Ready should fail");
    TEST_ASSERT(sm.getState() == PlaybackState::Ready, "State should still be Ready");

    std::cout << "enterSeeking rejection test passed!" << std::endl;
}

#ifdef STANDALONE_TEST
int main()
{
    testEnterExitSeekingFromPlaying();
    testEnterExitSeekingFromPaused();
    testEnterSeekingRejectedWhenNotPlayingOrPaused();
    std::cout << "All SeekingStateMachine tests passed!" << std::endl;
    return 0;
}
#endif
