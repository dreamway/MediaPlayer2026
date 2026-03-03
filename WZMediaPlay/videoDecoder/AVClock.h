#pragma once

#include "chronons.h"
#include <QElapsedTimer>
#include <QMutex>
#include <QObject>
#include <atomic>

/**
 * AVClock: 独立时钟类
 *
 * 职责：
 * - 管理播放时钟（音频时钟、视频时钟、外部时钟）
 * - 提供统一的时间获取接口
 * - 支持暂停、恢复、倍速播放
 * - 用于音视频同步
 *
 * 参考 QtAV 的 AVClock 设计
 */
class AVClock : public QObject
{
    Q_OBJECT

public:
    enum ClockType {
        AudioClock,    // 音频时钟（默认）
        VideoClock,    // 视频时钟
        ExternalClock  // 外部时钟
    };

    explicit AVClock(ClockType type = AudioClock, QObject *parent = nullptr);
    ~AVClock() override;

    // 时钟类型
    void setClockType(ClockType type);
    ClockType clockType() const { return clockType_; }

    // 自动时钟模式（根据是否有音频流自动选择）
    void setClockAuto(bool autoClock) { autoClock_ = autoClock; }
    bool isClockAuto() const { return autoClock_; }

    // 初始值（用于媒体起始时间不为0的情况）
    void setInitialValue(double seconds);
    double initialValue() const { return initialValue_; }

    // 更新时钟值（由音频或视频线程调用）
    void updateValue(double pts);
    void updateVideoTime(double pts);

    // 获取当前时钟值（秒）
    double value() const;
    double pts() const;
    double videoTime() const { return videoPts_; }

    // 延迟（音频播放消耗的时间）
    void updateDelay(double delay) { delay_ = delay; }
    double delay() const { return delay_; }

    // 速度（倍速播放）
    void setSpeed(double speed);
    double speed() const { return speed_; }

    // 暂停/恢复
    bool isPaused() const { return paused_; }

    // 启动/重置
    bool isActive() const;

public slots:
    void start();
    void pause(bool paused);
    void reset();

signals:
    void paused(bool paused);
    void resumed();
    void started();
    void resetted();

private:
    mutable QMutex mutex_;

    ClockType clockType_;
    bool autoClock_ = true;
    std::atomic<bool> paused_{false};
    std::atomic<bool> active_{false};

    // PTS 值
    double audioPts_ = 0.0;  // 音频 PTS
    bool hasAudioPts_ = false;  // 是否已收到音频 PTS（区分合法 PTS=0 和未初始化）
    double videoPts_ = 0.0;  // 视频 PTS
    double delay_ = 0.0;     // 延迟
    double initialValue_ = 0.0;  // 初始值

    // 外部时钟使用
    QElapsedTimer timer_;
    double speed_ = 1.0;

    // 统计
    mutable int nbUpdated_ = 0;
};
