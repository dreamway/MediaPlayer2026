// 单元测试：PlaybackStateMachine
// 增强版：覆盖播放器所有状态转换场景
// 基于 BUG-044 分析：状态转换是播放器核心功能，需要全面测试

#include "../PlaybackStateMachine.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

// 测试计数器
static int testsPassed = 0;
static int testsFailed = 0;
static std::mutex logMutex;

#define TEST_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            std::lock_guard<std::mutex> lock(logMutex); \
            std::cerr << "TEST FAILED: " << message << std::endl; \
            testsFailed++; \
            return false; \
        } \
    } while(0)

#define TEST_PASS(name) \
    do { \
        std::lock_guard<std::mutex> lock(logMutex); \
        std::cout << "[PASS] " << name << std::endl; \
        testsPassed++; \
    } while(0)

// ==================== 基础状态转换测试 ====================

bool testInitialState() {
    PlaybackStateMachine sm;
    TEST_ASSERT(sm.getState() == PlaybackState::Idle, "Initial state should be Idle");
    TEST_ASSERT(sm.isIdle(), "isIdle() should return true initially");
    TEST_ASSERT(!sm.isPlaying(), "isPlaying() should return false initially");
    TEST_ASSERT(!sm.isPaused(), "isPaused() should return false initially");
    TEST_ASSERT(!sm.isSeeking(), "isSeeking() should return false initially");
    TEST_ASSERT(!sm.isStopped(), "isStopped() should return false initially");
    TEST_PASS("InitialState");
    return true;
}

bool testNormalPlaybackFlow() {
    // 测试正常播放流程：Idle -> Opening -> Ready -> Playing
    PlaybackStateMachine sm;

    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Idle -> Opening should succeed");
    TEST_ASSERT(sm.isOpening(), "State should be Opening");

    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Opening -> Ready should succeed");
    TEST_ASSERT(sm.isReady(), "State should be Ready");

    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Ready -> Playing should succeed");
    TEST_ASSERT(sm.isPlaying(), "State should be Playing");

    TEST_PASS("NormalPlaybackFlow");
    return true;
}

bool testPlayPauseToggle() {
    // 测试播放/暂停切换：Playing <-> Paused
    PlaybackStateMachine sm;

    // 先进入 Playing 状态
    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);

    // Playing -> Paused
    TEST_ASSERT(sm.transitionTo(PlaybackState::Paused), "Playing -> Paused should succeed");
    TEST_ASSERT(sm.isPaused(), "State should be Paused");

    // Paused -> Playing
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Paused -> Playing should succeed");
    TEST_ASSERT(sm.isPlaying(), "State should be Playing");

    // 快速切换多次
    for (int i = 0; i < 5; ++i) {
        TEST_ASSERT(sm.transitionTo(PlaybackState::Paused), "Playing -> Paused (rapid toggle) should succeed");
        TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Paused -> Playing (rapid toggle) should succeed");
    }

    TEST_PASS("PlayPauseToggle");
    return true;
}

bool testSeekingFromPlaying() {
    // 测试从 Playing 状态进入和退出 Seeking
    PlaybackStateMachine sm;

    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);

    // 使用 enterSeeking/exitSeeking API
    TEST_ASSERT(sm.enterSeeking("test seek"), "enterSeeking from Playing should succeed");
    TEST_ASSERT(sm.isSeeking(), "State should be Seeking");
    TEST_ASSERT(sm.preSeekState() == PlaybackState::Playing, "preSeekState should be Playing");

    TEST_ASSERT(sm.exitSeeking("seek done"), "exitSeeking should succeed");
    TEST_ASSERT(sm.isPlaying(), "State should return to Playing after exitSeeking");

    TEST_PASS("SeekingFromPlaying");
    return true;
}

bool testSeekingFromPaused() {
    // 测试从 Paused 状态进入和退出 Seeking
    PlaybackStateMachine sm;

    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);
    sm.transitionTo(PlaybackState::Paused);

    TEST_ASSERT(sm.enterSeeking("seek while paused"), "enterSeeking from Paused should succeed");
    TEST_ASSERT(sm.isSeeking(), "State should be Seeking");
    TEST_ASSERT(sm.preSeekState() == PlaybackState::Paused, "preSeekState should be Paused");

    TEST_ASSERT(sm.exitSeeking("seek done"), "exitSeeking should succeed");
    TEST_ASSERT(sm.isPaused(), "State should return to Paused after exitSeeking");

    TEST_PASS("SeekingFromPaused");
    return true;
}

bool testStopFlow() {
    // 测试停止流程：Playing -> Stopping -> Stopped
    PlaybackStateMachine sm;

    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);

    TEST_ASSERT(sm.transitionTo(PlaybackState::Stopping), "Playing -> Stopping should succeed");
    TEST_ASSERT(sm.isStopping(), "State should be Stopping");

    TEST_ASSERT(sm.transitionTo(PlaybackState::Stopped), "Stopping -> Stopped should succeed");
    TEST_ASSERT(sm.isStopped(), "State should be Stopped");

    TEST_PASS("StopFlow");
    return true;
}

bool testReopenFromStopped() {
    // 测试从 Stopped 状态重新打开：Stopped -> Opening -> Ready -> Playing
    PlaybackStateMachine sm;

    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);
    sm.transitionTo(PlaybackState::Stopping);
    sm.transitionTo(PlaybackState::Stopped);

    // 从 Stopped 重新打开
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Stopped -> Opening should succeed");
    TEST_ASSERT(sm.isOpening(), "State should be Opening");

    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Opening -> Ready should succeed");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Ready -> Playing should succeed");

    TEST_PASS("ReopenFromStopped");
    return true;
}

bool testEofSeekScenario() {
    // 测试 EOF 后 Seek 场景：Stopped -> Seeking -> Ready
    // 这是 BUG-030 (EOF 后 Seek 失败) 相关的测试
    PlaybackStateMachine sm;

    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);
    sm.transitionTo(PlaybackState::Stopping);
    sm.transitionTo(PlaybackState::Stopped);

    // EOF 后用户 seek
    TEST_ASSERT(sm.enterSeeking("EOF seek"), "enterSeeking from Stopped should succeed");
    TEST_ASSERT(sm.isSeeking(), "State should be Seeking");
    TEST_ASSERT(sm.preSeekState() == PlaybackState::Stopped, "preSeekState should be Stopped");

    // exitSeeking 应该将状态恢复到 Ready（因为从 Stopped 来）
    TEST_ASSERT(sm.exitSeeking("EOF seek done"), "exitSeeking from EOF should succeed");
    TEST_ASSERT(sm.isReady(), "State should be Ready after EOF seek exit (not Stopped)");

    TEST_PASS("EofSeekScenario");
    return true;
}

bool testErrorRecovery() {
    // 测试错误状态和恢复
    PlaybackStateMachine sm;

    // 从任何状态都可以进入 Error
    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);

    TEST_ASSERT(sm.transitionTo(PlaybackState::Error), "Playing -> Error should succeed");
    TEST_ASSERT(sm.isError(), "State should be Error");

    // Error 可以恢复到 Stopped 或 Idle
    PlaybackStateMachine sm2;
    sm2.transitionTo(PlaybackState::Opening);
    sm2.transitionTo(PlaybackState::Error);
    TEST_ASSERT(sm2.transitionTo(PlaybackState::Stopped), "Error -> Stopped should succeed");

    PlaybackStateMachine sm3;
    sm3.transitionTo(PlaybackState::Opening);
    sm3.transitionTo(PlaybackState::Error);
    TEST_ASSERT(sm3.transitionTo(PlaybackState::Idle), "Error -> Idle should succeed");

    TEST_PASS("ErrorRecovery");
    return true;
}

bool testVideoSwitchFlow() {
    // 测试视频切换流程：Playing -> Stopping -> Stopped -> Opening -> Ready -> Playing
    PlaybackStateMachine sm;

    // 第一个视频播放
    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    sm.transitionTo(PlaybackState::Playing);

    // 切换视频：停止当前视频
    TEST_ASSERT(sm.transitionTo(PlaybackState::Stopping), "Playing -> Stopping (video switch) should succeed");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Stopped), "Stopping -> Stopped (video switch) should succeed");

    // 打开新视频
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Stopped -> Opening (new video) should succeed");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Opening -> Ready (new video) should succeed");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Ready -> Playing (new video) should succeed");

    TEST_PASS("VideoSwitchFlow");
    return true;
}

// ==================== 非法状态转换测试 ====================

bool testInvalidTransitions() {
    PlaybackStateMachine sm;

    // Idle 不能直接到 Playing/Paused/Seeking/Stopped
    TEST_ASSERT(!sm.transitionTo(PlaybackState::Playing), "Idle -> Playing should fail");
    TEST_ASSERT(!sm.transitionTo(PlaybackState::Paused), "Idle -> Paused should fail");
    TEST_ASSERT(!sm.transitionTo(PlaybackState::Seeking), "Idle -> Seeking should fail");
    TEST_ASSERT(!sm.transitionTo(PlaybackState::Stopped), "Idle -> Stopped should fail");
    TEST_ASSERT(sm.isIdle(), "State should remain Idle after failed transitions");

    // Ready 不能直接到 Seeking/Paused/Stopped
    PlaybackStateMachine smReady;
    smReady.transitionTo(PlaybackState::Opening);
    smReady.transitionTo(PlaybackState::Ready);
    TEST_ASSERT(!smReady.transitionTo(PlaybackState::Seeking), "Ready -> Seeking should fail");
    TEST_ASSERT(!smReady.transitionTo(PlaybackState::Paused), "Ready -> Paused should fail");
    TEST_ASSERT(!smReady.transitionTo(PlaybackState::Stopped), "Ready -> Stopped should fail");

    // Stopping 不能转换到 Playing/Paused/Seeking
    PlaybackStateMachine smStopping;
    smStopping.transitionTo(PlaybackState::Opening);
    smStopping.transitionTo(PlaybackState::Ready);
    smStopping.transitionTo(PlaybackState::Playing);
    smStopping.transitionTo(PlaybackState::Stopping);
    TEST_ASSERT(!smStopping.transitionTo(PlaybackState::Playing), "Stopping -> Playing should fail");
    TEST_ASSERT(!smStopping.transitionTo(PlaybackState::Paused), "Stopping -> Paused should fail");
    TEST_ASSERT(!smStopping.transitionTo(PlaybackState::Seeking), "Stopping -> Seeking should fail");

    TEST_PASS("InvalidTransitions");
    return true;
}

// ==================== 幂等性测试 ====================

bool testIdempotentTransitions() {
    // 测试相同状态转换（幂等性）
    PlaybackStateMachine sm;

    TEST_ASSERT(sm.transitionTo(PlaybackState::Idle), "Idle -> Idle should succeed (idempotent)");
    TEST_ASSERT(sm.isIdle(), "State should still be Idle");

    sm.transitionTo(PlaybackState::Opening);
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Opening -> Opening should succeed (idempotent)");
    TEST_ASSERT(sm.isOpening(), "State should still be Opening");

    sm.transitionTo(PlaybackState::Ready);
    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Ready -> Ready should succeed (idempotent)");

    sm.transitionTo(PlaybackState::Playing);
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Playing -> Playing should succeed (idempotent)");

    TEST_PASS("IdempotentTransitions");
    return true;
}

// ==================== 并发测试 ====================

void workerThread(PlaybackStateMachine* sm, int iterations, std::atomic<int>* successCount) {
    for (int i = 0; i < iterations; ++i) {
        // 模拟状态查询和修改
        PlaybackState state = sm->getState();

        // 随机尝试一些转换（可能成功也可能失败）
        if (state == PlaybackState::Idle) {
            if (sm->transitionTo(PlaybackState::Opening)) {
                successCount->fetch_add(1);
            }
        } else if (state == PlaybackState::Opening) {
            if (sm->transitionTo(PlaybackState::Ready)) {
                successCount->fetch_add(1);
            }
        } else if (state == PlaybackState::Ready) {
            if (sm->transitionTo(PlaybackState::Playing)) {
                successCount->fetch_add(1);
            }
        } else if (state == PlaybackState::Playing) {
            // 随机选择暂停或停止
            if (i % 2 == 0) {
                if (sm->transitionTo(PlaybackState::Paused)) {
                    successCount->fetch_add(1);
                }
            } else {
                if (sm->transitionTo(PlaybackState::Stopping)) {
                    successCount->fetch_add(1);
                }
            }
        } else if (state == PlaybackState::Paused) {
            if (sm->transitionTo(PlaybackState::Playing)) {
                successCount->fetch_add(1);
            }
        } else if (state == PlaybackState::Stopped) {
            if (sm->transitionTo(PlaybackState::Opening)) {
                successCount->fetch_add(1);
            }
        }
    }
}

bool testConcurrentAccess() {
    PlaybackStateMachine sm;
    std::atomic<int> successCount(0);

    const int numThreads = 4;
    const int iterations = 1000;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, &sm, iterations, &successCount);
    }

    for (auto& t : threads) {
        t.join();
    }

    // 应该有成功的转换（具体数量不确定）
    TEST_ASSERT(successCount.load() > 0, "Concurrent access should have successful transitions");

    TEST_PASS("ConcurrentAccess");
    return true;
}

// ==================== 回调测试 ====================

bool testStateChangeCallback() {
    PlaybackStateMachine sm;

    std::vector<std::string> transitions;
    sm.setStateChangeCallback([&transitions](PlaybackState oldState, PlaybackState newState, const std::string& reason) {
        std::string log = std::string(PlaybackStateMachine::stateName(oldState)) + " -> " +
                          PlaybackStateMachine::stateName(newState);
        if (!reason.empty()) {
            log += " (" + reason + ")";
        }
        transitions.push_back(log);
    });

    sm.transitionTo(PlaybackState::Opening, "test open");
    sm.transitionTo(PlaybackState::Ready, "test ready");
    sm.transitionTo(PlaybackState::Playing, "test play");

    TEST_ASSERT(transitions.size() == 3, "Should have 3 transition callbacks");
    TEST_ASSERT(transitions[0].find("Idle -> Opening") != std::string::npos, "First callback should be Idle -> Opening");
    TEST_ASSERT(transitions[0].find("test open") != std::string::npos, "First callback should have reason");
    TEST_ASSERT(transitions[1].find("Opening -> Ready") != std::string::npos, "Second callback should be Opening -> Ready");
    TEST_ASSERT(transitions[2].find("Ready -> Playing") != std::string::npos, "Third callback should be Ready -> Playing");

    TEST_PASS("StateChangeCallback");
    return true;
}

// ==================== 状态名称测试 ====================

bool testStateNames() {
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Idle)) == "Idle", "Idle name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Opening)) == "Opening", "Opening name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Ready)) == "Ready", "Ready name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Playing)) == "Playing", "Playing name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Paused)) == "Paused", "Paused name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Seeking)) == "Seeking", "Seeking name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Stopping)) == "Stopping", "Stopping name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Stopped)) == "Stopped", "Stopped name");
    TEST_ASSERT(std::string(PlaybackStateMachine::stateName(PlaybackState::Error)) == "Error", "Error name");

    TEST_PASS("StateNames");
    return true;
}

// ==================== 完整播放周期测试 ====================

bool testFullPlaybackCycle() {
    // 测试完整的播放周期：打开 -> 播放 -> 暂停 -> 恢复 -> Seek -> 停止 -> 关闭
    PlaybackStateMachine sm;

    // 打开
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Step 1: Open");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Ready), "Step 2: Ready");

    // 开始播放
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Step 3: Play");

    // 暂停
    TEST_ASSERT(sm.transitionTo(PlaybackState::Paused), "Step 4: Pause");

    // 恢复播放
    TEST_ASSERT(sm.transitionTo(PlaybackState::Playing), "Step 5: Resume");

    // Seek
    TEST_ASSERT(sm.enterSeeking("user seek"), "Step 6: Enter Seeking");
    TEST_ASSERT(sm.exitSeeking("seek done"), "Step 7: Exit Seeking");

    // 停止
    TEST_ASSERT(sm.transitionTo(PlaybackState::Stopping), "Step 8: Stopping");
    TEST_ASSERT(sm.transitionTo(PlaybackState::Stopped), "Step 9: Stopped");

    // 重新打开
    TEST_ASSERT(sm.transitionTo(PlaybackState::Opening), "Step 10: Reopen");

    TEST_PASS("FullPlaybackCycle");
    return true;
}

// ==================== 边界条件测试 ====================

bool testCanTransitionTo() {
    PlaybackStateMachine sm;

    // 测试 canTransitionTo 方法
    TEST_ASSERT(sm.canTransitionTo(PlaybackState::Opening), "Can transition to Opening from Idle");
    TEST_ASSERT(!sm.canTransitionTo(PlaybackState::Playing), "Cannot transition to Playing from Idle");
    TEST_ASSERT(!sm.canTransitionTo(PlaybackState::Seeking), "Cannot transition to Seeking from Idle");

    sm.transitionTo(PlaybackState::Opening);
    sm.transitionTo(PlaybackState::Ready);
    TEST_ASSERT(sm.canTransitionTo(PlaybackState::Playing), "Can transition to Playing from Ready");
    TEST_ASSERT(!sm.canTransitionTo(PlaybackState::Seeking), "Cannot transition to Seeking from Ready");

    sm.transitionTo(PlaybackState::Playing);
    TEST_ASSERT(sm.canTransitionTo(PlaybackState::Paused), "Can transition to Paused from Playing");
    TEST_ASSERT(sm.canTransitionTo(PlaybackState::Seeking), "Can transition to Seeking from Playing");
    TEST_ASSERT(sm.canTransitionTo(PlaybackState::Stopping), "Can transition to Stopping from Playing");

    TEST_PASS("CanTransitionTo");
    return true;
}

// ==================== 主函数 ====================

int main() {
    std::cout << "======================================" << std::endl;
    std::cout << "PlaybackStateMachine Unit Tests" << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << std::endl;

    // 基础测试
    std::cout << "--- Basic Tests ---" << std::endl;
    testInitialState();
    testNormalPlaybackFlow();
    testPlayPauseToggle();
    testSeekingFromPlaying();
    testSeekingFromPaused();
    testStopFlow();
    testReopenFromStopped();
    testEofSeekScenario();
    testErrorRecovery();
    testVideoSwitchFlow();

    // 高级测试
    std::cout << std::endl << "--- Advanced Tests ---" << std::endl;
    testInvalidTransitions();
    testIdempotentTransitions();
    testConcurrentAccess();
    testStateChangeCallback();
    testStateNames();
    testFullPlaybackCycle();
    testCanTransitionTo();

    // 结果汇总
    std::cout << std::endl;
    std::cout << "======================================" << std::endl;
    std::cout << "Results: " << testsPassed << " passed, " << testsFailed << " failed" << std::endl;
    std::cout << "======================================" << std::endl;

    return testsFailed > 0 ? 1 : 0;
}