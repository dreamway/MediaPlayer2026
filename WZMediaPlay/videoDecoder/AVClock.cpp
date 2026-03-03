#include "AVClock.h"
#include "../GlobalDef.h"
#include <spdlog/spdlog.h>

extern spdlog::logger *logger;

static constexpr double kThousandth = 0.001;

AVClock::AVClock(ClockType type, QObject *parent)
    : QObject(parent)
    , clockType_(type)
    , autoClock_(true)
    , paused_(false)
    , active_(false)
    , audioPts_(0.0)
    , videoPts_(0.0)
    , delay_(0.0)
    , initialValue_(0.0)
    , speed_(1.0)
    , nbUpdated_(0)
{
    SPDLOG_LOGGER_DEBUG(logger, "AVClock created, type: {}", 
        type == AudioClock ? "Audio" : (type == VideoClock ? "Video" : "External"));
}

AVClock::~AVClock()
{
    SPDLOG_LOGGER_DEBUG(logger, "AVClock destroyed");
}

void AVClock::setClockType(ClockType type)
{
    QMutexLocker locker(&mutex_);
    if (clockType_ == type) {
        return;
    }
    
    clockType_ = type;
    
    // 切换时钟类型时重启计时器
    if (active_ && !paused_) {
        timer_.restart();
    }
    
    SPDLOG_LOGGER_INFO(logger, "AVClock type changed to: {}", 
        type == AudioClock ? "Audio" : (type == VideoClock ? "Video" : "External"));
}

void AVClock::setInitialValue(double seconds)
{
    QMutexLocker locker(&mutex_);
    initialValue_ = seconds;
    SPDLOG_LOGGER_INFO(logger, "AVClock initial value set to: {:.3f}s", seconds);
}

void AVClock::updateValue(double pts)
{
    QMutexLocker locker(&mutex_);
    
    if (clockType_ == AudioClock) {
        audioPts_ = pts;
        hasAudioPts_ = true;
        nbUpdated_++;
    }
    // 其他时钟类型不更新 audioPts_
}

void AVClock::updateVideoTime(double pts)
{
    QMutexLocker locker(&mutex_);
    
    videoPts_ = pts;
    
    if (clockType_ == VideoClock && timer_.isValid()) {
        timer_.restart();
    }
    
    nbUpdated_++;
}

double AVClock::value() const
{
    QMutexLocker locker(&mutex_);
    
    switch (clockType_) {
    case AudioClock:
        // 音频时钟：未收到 PTS 时用 initialValue（如 seek 后），否则用 audioPts_ + delay_
        // 使用 hasAudioPts_ 标志区分"尚未收到 PTS"和"合法 PTS=0"
        return (!hasAudioPts_ ? initialValue_ : audioPts_ + delay_);
        
    case ExternalClock:
        // 外部时钟：基于计时器
        if (timer_.isValid() && !paused_) {
            // 累加经过的时间
            double elapsed = timer_.elapsed() * kThousandth * speed_;
            return audioPts_ + elapsed + initialValue_;
        } else {
            // 暂停状态返回暂停时的值
            return audioPts_ + initialValue_;
        }
        
    case VideoClock:
        // 视频时钟：直接使用视频 pts
        return videoPts_ + initialValue_;
        
    default:
        return initialValue_;
    }
}

double AVClock::pts() const
{
    QMutexLocker locker(&mutex_);
    
    switch (clockType_) {
    case AudioClock:
        return audioPts_;
    case VideoClock:
        return videoPts_;
    case ExternalClock:
        return audioPts_;
    default:
        return 0.0;
    }
}

void AVClock::setSpeed(double speed)
{
    QMutexLocker locker(&mutex_);
    
    if (speed <= 0.0) {
        SPDLOG_LOGGER_WARN(logger, "AVClock::setSpeed: invalid speed: {}", speed);
        return;
    }
    
    // 如果正在运行，先更新当前 pts
    if (timer_.isValid() && !paused_ && clockType_ == ExternalClock) {
        double elapsed = timer_.elapsed() * kThousandth * speed_;
        audioPts_ += elapsed;
    }
    
    speed_ = speed;
    
    // 重启计时器
    if (timer_.isValid() && !paused_) {
        timer_.restart();
    }
    
    SPDLOG_LOGGER_INFO(logger, "AVClock speed set to: {:.2f}x", speed);
}

bool AVClock::isActive() const
{
    QMutexLocker locker(&mutex_);
    
    if (clockType_ == AudioClock) {
        return active_;  // 音频时钟只要有活动标记即可
    } else {
        return timer_.isValid();
    }
}

void AVClock::start()
{
    QMutexLocker locker(&mutex_);
    
    active_ = true;
    paused_ = false;
    
    if (clockType_ == ExternalClock) {
        timer_.start();
    }
    
    SPDLOG_LOGGER_INFO(logger, "AVClock started, type: {}", 
        clockType_ == AudioClock ? "Audio" : (clockType_ == VideoClock ? "Video" : "External"));
    
    emit started();
}

void AVClock::pause(bool paused)
{
    QMutexLocker locker(&mutex_);
    
    if (paused_ == paused) {
        return;
    }
    
    paused_ = paused;
    
    if (clockType_ == ExternalClock) {
        if (paused) {
            // 暂停时记录当前值
            if (timer_.isValid()) {
                double elapsed = timer_.elapsed() * kThousandth * speed_;
                audioPts_ += elapsed;
            }
            // 计时器不停止，只是不再累加
        } else {
            // 恢复时重启计时器
            timer_.restart();
        }
    }
    
    SPDLOG_LOGGER_INFO(logger, "AVClock {}", paused ? "paused" : "resumed");
    
    // 发射信号（使用 this-> 避免与参数名冲突）
    bool isPaused = paused;
    emit this->paused(isPaused);
    if (!isPaused) {
        emit this->resumed();
    }
}

void AVClock::reset()
{
    QMutexLocker locker(&mutex_);
    
    audioPts_ = 0.0;
    hasAudioPts_ = false;
    videoPts_ = 0.0;
    delay_ = 0.0;
    nbUpdated_ = 0;
    
    if (clockType_ == ExternalClock) {
        timer_.invalidate();
    }
    
    // 不重置 initialValue_ 和 speed_
    
    SPDLOG_LOGGER_INFO(logger, "AVClock reset");
    
    emit resetted();
}
