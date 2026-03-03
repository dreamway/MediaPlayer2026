#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <functional>
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

/**
 * PlaybackStateMachine: 播放状态机
 * 
 * 统一管理播放状态，确保状态转换的正确性和原子性
 * 参考QMPlayer2的状态管理机制
 */
enum class PlaybackState {
    Idle,           // 空闲状态（初始状态）
    Opening,        // 正在打开文件
    Ready,          // 文件已打开，准备播放
    Playing,        // 正在播放
    Paused,         // 已暂停
    Seeking,        // 正在Seeking
    Stopping,       // 正在停止
    Stopped,        // 已停止
    Error           // 错误状态
};

class PlaybackStateMachine {
public:
    PlaybackStateMachine();
    ~PlaybackStateMachine() = default;

    // 状态查询
    PlaybackState getState() const;
    bool isIdle() const;
    bool isOpening() const;
    bool isReady() const;
    bool isPlaying() const;
    bool isPaused() const;
    bool isSeeking() const;
    bool isStopping() const;
    bool isStopped() const;
    bool isError() const;

    // 状态转换
    bool transitionTo(PlaybackState newState);
    bool transitionTo(PlaybackState newState, const std::string& reason);

    // 状态转换（带验证）
    bool canTransitionTo(PlaybackState newState) const;
    
    // 获取状态名称（用于日志）
    static const char* stateName(PlaybackState state);
    
    // 设置状态变化回调（用于调试和监控）
    void setStateChangeCallback(std::function<void(PlaybackState, PlaybackState, const std::string&)> callback);

private:
    // 验证状态转换是否合法
    bool isValidTransition(PlaybackState from, PlaybackState to) const;
    
    // 状态
    mutable std::mutex stateMutex_;
    std::atomic<PlaybackState> currentState_;
    
    // 状态变化回调
    std::function<void(PlaybackState, PlaybackState, const std::string&)> stateChangeCallback_;
};
