#include "ThreadSyncManager.h"
#include <algorithm>
#include <thread>

ThreadSyncManager::ThreadSyncManager() {
}

bool ThreadSyncManager::tryLock(std::mutex& mutex, std::chrono::milliseconds timeout) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        if (mutex.try_lock()) {
            lockCount_++;
            return true;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
        
        if (elapsed >= timeout) {
            if (logger) {
                logger->warn("ThreadSyncManager: Lock timeout after {}ms", timeout.count());
            }
            deadlockCount_++;
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

bool ThreadSyncManager::tryLockMultiple(std::vector<std::mutex*> mutexes, std::chrono::milliseconds timeout) {
    // 按地址排序，避免死锁
    std::sort(mutexes.begin(), mutexes.end());
    
    auto start = std::chrono::steady_clock::now();
    std::vector<std::mutex*> locked;
    
    for (auto* mutex : mutexes) {
        if (!mutex) {
            continue;
        }
        
        while (true) {
            if (mutex->try_lock()) {
                locked.push_back(mutex);
                lockCount_++;
                break;
            }
            
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            
            if (elapsed >= timeout) {
                // 超时，释放已获取的锁
                for (auto* m : locked) {
                    m->unlock();
                }
                if (logger) {
                    logger->warn("ThreadSyncManager: Multiple lock timeout after {}ms", timeout.count());
                }
                deadlockCount_++;
                return false;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    
    return true;
}

void ThreadSyncManager::unlock(std::mutex& mutex) {
    mutex.unlock();
    lockCount_--;
}

void ThreadSyncManager::unlockMultiple(std::vector<std::mutex*> mutexes) {
    for (auto* mutex : mutexes) {
        if (mutex) {
            mutex->unlock();
            lockCount_--;
        }
    }
}

bool ThreadSyncManager::detectDeadlock() {
    // 简单的死锁检测：如果锁计数异常高，可能发生死锁
    if (lockCount_.load() > 10) {
        if (logger) {
            logger->warn("ThreadSyncManager: Possible deadlock detected (lock count: {})", lockCount_.load());
        }
        return true;
    }
    return false;
}
