#include "AVThread.h"
#include "PlayController.h"
#include "Decoder.h"

#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

AVThread::AVThread(PlayController* controller)
    : controller_(controller)
{
    connect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    mutex_.lock();  // 初始锁定，确保线程启动前不会执行
}

AVThread::~AVThread()
{
    // 注意：mutex是在构造函数中（主线程）锁定的
    // 如果线程还在运行，mutex应该在stop()方法中（主线程）解锁
    // 如果线程已经停止，mutex可能还在锁定状态，需要在析构函数中（主线程）解锁
    // 但只有在当前线程是主线程时才解锁，避免跨线程解锁
    
    // 检查mutex是否还在锁定状态
    // 如果tryLock成功，说明mutex未锁定，可以安全退出
    if (mutex_.tryLock(0)) {
        // 立即解锁（因为我们只是想检查状态）
        mutex_.unlock();
    } else {
        // tryLock失败，说明mutex被锁定
        // 只有在主线程中才尝试解锁（避免跨线程解锁）
        // 注意：QThread::currentThread() == this 表示当前线程是此线程对象所在的线程
        // 但析构函数通常在主线程中调用，所以可以安全解锁
        try {
            mutex_.unlock();
            if (logger) {
                logger->debug("AVThread::~AVThread: Unlocked mutex in destructor");
            }
        } catch (...) {
            // 解锁失败，记录警告但不抛出异常
            if (logger) {
                logger->warn("AVThread::~AVThread: Failed to unlock mutex in destructor");
            }
        }
    }
    
    // 注意：不在这里删除 dec_，因为子类可能使用不同的管理方式
    // VideoThread 的 dec_ 由 PlayController 管理
    // AudioThread 可能不使用 Decoder 接口
}

void AVThread::maybeStartThread()
{
    // 子类实现：检查 writer 是否准备好
    // VideoThread 检查 videoWriter_
    // AudioThread 检查 audio_
}

void AVThread::setDec(Decoder* dec)
{
    dec_ = dec;
    if (logger) {
        logger->debug("AVThread::setDec: decoder set, ptr: {}", (void*)dec_);
    }
}

void AVThread::destroyDec()
{
    // 注意：不在这里删除 dec_，因为 dec_ 可能由 PlayController 管理
    // 子类可以重写此方法
    dec_ = nullptr;
    if (logger) {
        logger->debug("AVThread::destroyDec: decoder cleared");
    }
}

bool AVThread::lock()
{
    br2_ = true;
    if (!mutex_.tryLock(MUTEXWAIT_TIMEOUT))
    {
        // 等待更长时间
        const bool ret = mutex_.tryLock(MUTEXWAIT_TIMEOUT * 2);
        if (!ret)
        {
            br2_ = false;
            if (logger) {
                logger->warn("AVThread::lock: failed to acquire lock");
            }
            return false;
        }
    }
    return true;
}

void AVThread::unlock()
{
    br2_ = false;
    mutex_.unlock();
}

void AVThread::stop(bool terminate)
{
    if (terminate)
        return this->terminate();

    br_ = true;
    // 在主线程中解锁mutex（mutex是在构造函数中主线程锁定的）
    // 这允许run()方法继续执行并检查br_标志后退出
    mutex_.unlock();
    
    // 唤醒等待的线程（通过PlayController的emptyBufferCond）
    if (controller_) {
        controller_->wakeAllThreads();  // PlayController需要实现此方法
    }

    if (!wait(TERMINATE_TIMEOUT))
        this->terminate();
}

bool AVThread::hasError() const
{
    return false;
}

bool AVThread::hasDecoderError() const
{
    return false;
}

void AVThread::terminate()
{
    // 断开finished()信号连接，避免Qt自动删除对象
    // 调用者（PlayController）会手动管理线程的生命周期
    disconnect(this, SIGNAL(finished()), this, SLOT(deleteLater()));
    QThread::terminate();
    wait(1000);
    if (logger) {
        logger->warn("AVThread::terminate: thread has been incorrectly terminated!");
    }
}
