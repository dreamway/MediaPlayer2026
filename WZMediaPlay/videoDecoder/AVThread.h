#pragma once

#include <QThread>
#include <QMutex>
#include <QRecursiveMutex>
#include <QWaitCondition>

class PlayController;  // Forward declaration
class Decoder;

/**
 * AVThread: 音视频线程基类（参考 QMPlayer2 的 AVThread）
 * 
 * 职责：
 * - 统一管理 Decoder 和线程同步
 * - 提供 lock()/unlock() 机制确保线程安全
 * - 提供 stop() 方法优雅停止线程
 * 
 * 设计：
 * - VideoThread 和 AudioThread 继承此基类
 * - 使用 QRecursiveMutex 支持递归锁定
 * - 使用 updateMutex 保护更新操作
 */
class AVThread : public QThread
{
    Q_OBJECT

public:
    virtual void setDec(Decoder* dec);
    void destroyDec();

    inline bool updateTryLock()
    {
        return updateMutex_.tryLock();
    }
    inline void updateUnlock()
    {
        updateMutex_.unlock();
    }

    virtual bool lock();
    virtual void unlock();

    inline bool isWaiting() const
    {
        return waiting_;
    }

    virtual void stop(bool terminate = false);

    virtual bool hasError() const;
    virtual bool hasDecoderError() const;

    Decoder* dec_ = nullptr;

protected:
    AVThread(PlayController* controller);
    virtual ~AVThread();

    void maybeStartThread();

    void terminate();

    PlayController* controller_;

    volatile bool br_ = false, br2_ = false;
    bool waiting_ = false;
    QRecursiveMutex mutex_;
    QMutex updateMutex_;
    int mutexLockCount_ = 0;  // 追踪 mutex_ 的锁计数（用于安全析构）

private:
    static const int MUTEXWAIT_TIMEOUT = 1000;  // 1秒超时
    static const int TERMINATE_TIMEOUT = 5000;  // 5秒超时
};
