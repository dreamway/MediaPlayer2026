#pragma once

#include <mutex>
#include <atomic>
#include <chrono>
#include <vector>
#include <memory>
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

/**
 * ThreadSyncManager: 线程同步管理器
 * 
 * 统一管理线程同步，避免死锁和资源竞争
 * 提供超时和死锁检测功能
 */
class ThreadSyncManager {
public:
    ThreadSyncManager();
    ~ThreadSyncManager() = default;

    // 获取锁（带超时）
    bool tryLock(std::mutex& mutex, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
    
    // 获取多个锁（按顺序，避免死锁）
    bool tryLockMultiple(std::vector<std::mutex*> mutexes, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000));
    
    // 释放锁
    void unlock(std::mutex& mutex);
    void unlockMultiple(std::vector<std::mutex*> mutexes);
    
    // 死锁检测
    bool detectDeadlock();
    
    // 获取锁统计信息
    int getLockCount() const { return lockCount_.load(); }
    int getDeadlockCount() const { return deadlockCount_.load(); }

    // RAII ScopedLock — 简化 ThreadSyncManager 的使用
    // 用法：auto lock = tsm.scopedLock(mutex, timeout);
    //       if (lock) { ... 已持锁 ... }
    class ScopedLock {
    public:
        ScopedLock(ThreadSyncManager &tsm, std::mutex &mtx, bool acquired)
            : tsm_(tsm), mtx_(mtx), acquired_(acquired) {}
        ~ScopedLock() { if (acquired_) tsm_.unlock(mtx_); }
        ScopedLock(const ScopedLock &) = delete;
        ScopedLock &operator=(const ScopedLock &) = delete;
        explicit operator bool() const { return acquired_; }
    private:
        ThreadSyncManager &tsm_;
        std::mutex &mtx_;
        bool acquired_;
    };

    ScopedLock scopedLock(std::mutex &mutex, std::chrono::milliseconds timeout = std::chrono::milliseconds(1000))
    {
        bool acquired = tryLock(mutex, timeout);
        return ScopedLock(*this, mutex, acquired);
    }

private:
    // 锁计数
    std::atomic<int> lockCount_{0};
    std::atomic<int> deadlockCount_{0};
    
    // 锁超时时间
    std::chrono::milliseconds defaultTimeout_{1000};
};
