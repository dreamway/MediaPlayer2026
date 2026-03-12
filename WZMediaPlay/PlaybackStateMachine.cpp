#include "PlaybackStateMachine.h"
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

PlaybackStateMachine::PlaybackStateMachine()
    : currentState_(PlaybackState::Idle)
{
}

PlaybackState PlaybackStateMachine::getState() const {
    return currentState_.load(std::memory_order_acquire);
}

bool PlaybackStateMachine::isIdle() const {
    return getState() == PlaybackState::Idle;
}

bool PlaybackStateMachine::isOpening() const {
    return getState() == PlaybackState::Opening;
}

bool PlaybackStateMachine::isReady() const {
    return getState() == PlaybackState::Ready;
}

bool PlaybackStateMachine::isPlaying() const {
    return getState() == PlaybackState::Playing;
}

bool PlaybackStateMachine::isPaused() const {
    return getState() == PlaybackState::Paused;
}

bool PlaybackStateMachine::isSeeking() const {
    return getState() == PlaybackState::Seeking;
}

bool PlaybackStateMachine::isStopping() const {
    return getState() == PlaybackState::Stopping;
}

bool PlaybackStateMachine::isStopped() const {
    return getState() == PlaybackState::Stopped;
}

bool PlaybackStateMachine::isError() const {
    return getState() == PlaybackState::Error;
}

bool PlaybackStateMachine::canTransitionTo(PlaybackState newState) const {
    PlaybackState current = getState();
    return isValidTransition(current, newState);
}

bool PlaybackStateMachine::transitionTo(PlaybackState newState) {
    return transitionTo(newState, "");
}

bool PlaybackStateMachine::transitionTo(PlaybackState newState, const std::string& reason) {
    PlaybackState oldState = getState();
    
    // 检查状态转换是否合法
    if (!isValidTransition(oldState, newState)) {
        if (logger) {
            logger->warn("PlaybackStateMachine: Invalid state transition from {} to {} (reason: {})",
                        stateName(oldState), stateName(newState), reason);
        }
        return false;
    }
    
    // 执行状态转换
    currentState_.store(newState, std::memory_order_release);
    
    if (logger) {
        SPDLOG_LOGGER_INFO(logger,"PlaybackStateMachine: State transition {} -> {} (reason: {})",
                    stateName(oldState), stateName(newState), reason.empty() ? "none" : reason);
    }
    
    // 调用回调
    if (stateChangeCallback_) {
        stateChangeCallback_(oldState, newState, reason);
    }
    
    return true;
}

bool PlaybackStateMachine::isValidTransition(PlaybackState from, PlaybackState to) const {
    // 相同状态转换是合法的（幂等性）
    if (from == to) {
        return true;
    }
    
    // 定义合法的状态转换
    switch (from) {
        case PlaybackState::Idle:
            return to == PlaybackState::Opening || to == PlaybackState::Error;
            
        case PlaybackState::Opening:
            return to == PlaybackState::Ready || to == PlaybackState::Error || to == PlaybackState::Stopping;
            
        case PlaybackState::Ready:
            return to == PlaybackState::Playing || to == PlaybackState::Stopping || to == PlaybackState::Error;
            
        case PlaybackState::Playing:
            return to == PlaybackState::Paused || 
                   to == PlaybackState::Seeking || 
                   to == PlaybackState::Stopping || 
                   to == PlaybackState::Error;
            
        case PlaybackState::Paused:
            return to == PlaybackState::Playing || 
                   to == PlaybackState::Seeking || 
                   to == PlaybackState::Stopping || 
                   to == PlaybackState::Error;
            
        case PlaybackState::Seeking:
            return to == PlaybackState::Playing || 
                   to == PlaybackState::Paused || 
                   to == PlaybackState::Stopping || 
                   to == PlaybackState::Error;
            
        case PlaybackState::Stopping:
            return to == PlaybackState::Stopped || to == PlaybackState::Error;
            
        case PlaybackState::Stopped:
            // 允许转换到 Seeking（用于 EOF 后重新 seek 的场景）
            return to == PlaybackState::Idle || to == PlaybackState::Opening ||
                   to == PlaybackState::Ready || to == PlaybackState::Seeking ||
                   to == PlaybackState::Error;
            
        case PlaybackState::Error:
            return to == PlaybackState::Stopped || to == PlaybackState::Idle;
            
        default:
            return false;
    }
}

const char* PlaybackStateMachine::stateName(PlaybackState state) {
    switch (state) {
        case PlaybackState::Idle: return "Idle";
        case PlaybackState::Opening: return "Opening";
        case PlaybackState::Ready: return "Ready";
        case PlaybackState::Playing: return "Playing";
        case PlaybackState::Paused: return "Paused";
        case PlaybackState::Seeking: return "Seeking";
        case PlaybackState::Stopping: return "Stopping";
        case PlaybackState::Stopped: return "Stopped";
        case PlaybackState::Error: return "Error";
        default: return "Unknown";
    }
}

bool PlaybackStateMachine::enterSeeking(const std::string& reason)
{
    PlaybackState current = getState();
    // 允许从 Playing、Paused、Stopped 状态进入 Seeking
    // Stopped 状态用于 EOF 后重新 seek 的场景
    if (current != PlaybackState::Playing &&
        current != PlaybackState::Paused &&
        current != PlaybackState::Stopped) {
        if (logger) {
            logger->warn("PlaybackStateMachine::enterSeeking: Cannot seek from state {} (reason: {})",
                        stateName(current), reason);
        }
        return false;
    }
    preSeekState_.store(current, std::memory_order_release);
    return transitionTo(PlaybackState::Seeking, reason.empty() ? "enterSeeking" : reason);
}

bool PlaybackStateMachine::exitSeeking(const std::string& reason)
{
    if (!isSeeking()) {
        return false;
    }
    PlaybackState target = preSeekState_.load(std::memory_order_acquire);
    // 从 Stopped 状态来的，退出 seeking 后应该回到 Ready 状态（准备播放）
    if (target == PlaybackState::Stopped) {
        target = PlaybackState::Ready;
    } else if (target != PlaybackState::Playing && target != PlaybackState::Paused) {
        target = PlaybackState::Playing;
    }
    return transitionTo(target, reason.empty() ? "exitSeeking" : reason);
}

void PlaybackStateMachine::setStateChangeCallback(
    std::function<void(PlaybackState, PlaybackState, const std::string&)> callback) {
    stateChangeCallback_ = callback;
}
