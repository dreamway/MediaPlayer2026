#pragma once

#include "GlobalDef.h"
#include "PlaybackStateMachine.h"
#include "ThreadSyncManager.h"
#include "videoDecoder/Demuxer.h"
#include "videoDecoder/chronons.h" // 包含 nanoseconds, microseconds
#include "videoDecoder/ffmpeg.h"   // 包含 SyncMaster、get_avtime 和 AVFrame
#include "videoDecoder/packet_queue.h"
#include "videoDecoder/AVClock.h"  // AVClock 时钟类

// 前向声明，避免循环依赖
class Audio;
class VideoThread;
class AudioThread;
class DemuxerThread;
class Decoder;
class VideoRenderer;
class VideoWidgetBase;
class SystemMonitor;
class QWidget;
namespace videoDecoder { class Statistics; }

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <QCoreApplication>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QWaitCondition>

/**
 * PlayController: 播放控制器（参考 QMPlayer2 的 PlayClass）
 * 
 * 职责：
 * - 协调所有线程（DemuxerThread、VideoThread、AudioThread）
 * - 管理播放状态
 * - 处理用户命令（play/pause/stop/seek）
 * - 管理共享数据结构（Demuxer、PacketQueue）
 * 
 * 当前状态：
 * - 正在逐步迁移 Movie 的功能
 * - DemuxerThread 已创建，正在集成
 * - Video 和 Audio 仍通过 Movie 管理（后续会迁移为 VideoThread 和 AudioThread）
 */
class PlayController : public QObject
{
    Q_OBJECT

public:
    explicit PlayController(QObject *parent = nullptr);
    ~PlayController();

    // 播放控制
    bool open(const QString &filename);
    void play();
    void pause();
    void stop();
    bool seek(int64_t positionMs);
    bool canSeek() const;  // 检查是否可以进行 seek 操作

    // 其他接口
    int64_t getDurationMs() const;
    int64_t getCurrentPositionMs() const;
    void setVolume(float volume);
    float getVolume() const;
    void toggleMute();

    // 播放速度控制
    bool setPlaybackRate(double rate);
    double getPlaybackRate() const;
    float getVideoFrameRate() const;

    // 线程健康检查
    bool isThreadHealthy() const;
    void wakeAllThreads();

    // 断开线程信号连接（避免Qt自动删除对象）
    void disconnectThreadSignals(QThread *thread);

    // 辅助方法：安全停止线程（用于stop()方法）
    void stopThread(QThread *thread, const char *threadName, int timeoutMs);

    // 检测是否播放完成
    bool isPlaybackFinished(int64_t currentElapsedSeconds) const;

    // 检查并主动停止播放（如果检测到播放完成）
    bool checkAndStopIfFinished(int64_t currentElapsedSeconds);

    // 时钟同步相关（使用 AVClock）
    nanoseconds getMasterClock() const;  // 获取主时钟值（兼容旧接口，内部使用 AVClock）
    
    // AVClock 访问接口（供 VideoThread 和 AudioThread 调用）
    AVClock *getAVClock() { return masterClock_.get(); }  // 获取 AVClock 指针
    const AVClock *getAVClock() const { return masterClock_.get(); }  // 获取 AVClock 指针（const版本）
    bool isAudioFinished() const; // Video 需要此方法

    // Video 和 Audio 需要的接口（用于替代对 Movie 的直接访问）
    // 注意：quit_ 标志已统一到状态机，使用 isStopping() 或 isStopped() 检查
    std::chrono::steady_clock::time_point &getLastVideoFrameTime() { return lastVideoFrameTime_; } // Video 需要更新健康检查时间戳
    std::chrono::steady_clock::time_point &getLastAudioFrameTime() { return lastAudioFrameTime_; } // Audio 需要更新健康检查时间戳
    std::chrono::steady_clock::time_point &getLastDemuxTime() { return lastDemuxTime_; }           // DemuxerThread 需要更新健康检查时间戳

    // 视频时钟管理（供 VideoThread 调用）
    void updateVideoClock(nanoseconds pts); // 更新视频时钟（基于最后渲染的帧的 PTS）
    nanoseconds getVideoClock() const;      // 获取视频时钟

    // 全局基准 PTS 管理（供 VideoThread 和 AudioThread 调用）
    void setBasePts(nanoseconds pts);       // 设置全局基准 PTS
    nanoseconds getBasePts() const;         // 获取全局基准 PTS

    // 全局视频基准 PTS 管理（确保视频和音频使用相同基准）
    void setVideoBasePts(nanoseconds pts);  // 设置全局视频基准 PTS
    nanoseconds getVideoBasePts() const;    // 获取全局视频基准 PTS

    // DemuxerThread 需要的接口（用于访问 Audio，Video 类已删除）
    Audio *getAudio() const { return audio_; }

    // 获取视频 PacketQueue（用于 VideoThread 和 DemuxerThread）
    PacketQueue<50 * 1024 * 1024> *getVideoPacketQueue() { return &vPackets_; }
    PacketQueue<4 * 1024 * 1024> *getAudioPacketQueue() { return &aPackets_; }

    // 获取视频渲染窗口 widget（用于集成到 GUI）
    // 注意：返回的是 VideoWidgetBase，支持新的渲染架构
    QWidget *getVideoWidget() const;

    // 设置 VideoRenderer（新的渲染器接口）
    void setVideoRenderer(std::shared_ptr<VideoRenderer> renderer);
    VideoRenderer *getVideoRenderer() const { return videoRenderer_.get(); }

    // 统计信息
    void setStatistics(std::shared_ptr<videoDecoder::Statistics> stats);
    videoDecoder::Statistics *getStatistics() const { return statistics_.get(); }

    // 设置 VideoWidgetBase（新的视频窗口基类）
    void setVideoWidgetBase(VideoWidgetBase *widget);
    VideoWidgetBase *getVideoWidgetBase() const { return videoWidgetBase_; }

    // 获取状态机引用（用于各个组件检查播放状态）
    const PlaybackStateMachine &getStateMachine() const { return stateMachine_; }

    // 获取当前播放状态
    PlaybackState getPlaybackState() const { return stateMachine_.getState(); }

    // 状态检查方法（内联定义）
    bool isPlaying() const { return stateMachine_.isPlaying(); }
    bool isPaused() const { return stateMachine_.isPaused(); }
    bool isStopped() const { return stateMachine_.isStopped(); }
    bool isSeeking() const { return stateMachine_.isSeeking(); }
    bool isStopping() const { return stateMachine_.isStopping(); }
    bool isOpened() const
    {
        // 判断是否打开：有 Demuxer 且已打开
        return demuxer_ && demuxer_->isOpened();
    }

    // 获取线程同步管理器（供 DemuxerThread 使用）
    ThreadSyncManager &getThreadSyncManager() { return threadSyncManager_; }
    const ThreadSyncManager &getThreadSyncManager() const { return threadSyncManager_; }

    // Seeking 机制相关方法（供 VideoThread 和 AudioThread 调用）
    bool getFlushVideo() const { return flushVideo_; }
    void setFlushVideo(bool flush) { flushVideo_ = flush; }
    bool getFlushAudio() const { return flushAudio_; }
    void setFlushAudio(bool flush) { flushAudio_ = flush; }
    double getVideoSeekPos() const { return videoSeekPos_; }
    void setVideoSeekPos(double pos) { videoSeekPos_ = pos; }
    double getAudioSeekPos() const { return audioSeekPos_; }
    void setAudioSeekPos(double pos) { audioSeekPos_ = pos; }
    QWaitCondition &getEmptyBufferCond() { return emptyBufferCond_; }

    // 动态同步阈值方法
    nanoseconds getSyncThreshold() const { return syncThreshold_; }
    nanoseconds getMaxSyncThreshold() const { return maxSyncThreshold_; }
    void updateSyncThreshold(nanoseconds currentError);

signals:
    void playbackStateChanged(PlaybackState state);
    void seekingFinished(int64_t seekTarget);
    void playbackCompleted();  // 播放完成信号，用于播放列表自动切换

private slots:
    void onDemuxerThreadSeekFinished(int64_t positionUs);
    void onDemuxerThreadEofReached();
    void onDemuxerThreadErrorOccurred(int errorCode);
    void onPlaybackCompleted();


private:
    // 解码器初始化（从 Movie 迁移）
    bool initializeCodecs();
    int streamComponentOpen(unsigned int stream_index);

    Audio *audio_;

    // PacketQueue（视频和音频数据包队列，由 PlayController 直接管理）
    PacketQueue<50 * 1024 * 1024> vPackets_; // 视频数据包队列（增大到50MB以支持高分辨率视频）
    PacketQueue<4 * 1024 * 1024> aPackets_;  // 音频数据包队列（增大到4MB以避免快速填满）

    AVStream *videoStream_{nullptr};
    AVCodecContextPtr videoCodecCtx_; // 视频编解码器上下文

    // 音频流信息（从 Audio 类迁移，用于 streamComponentOpen）
    AVStream *audioStream_{nullptr};
    AVCodecContextPtr audioCodecCtx_; // 音频编解码器上下文

    // Demuxer（用于打开文件和获取流信息）
    std::unique_ptr<Demuxer> demuxer_;

    bool enableHardwareDecoding_; // 是否启用硬件解码（FFDecHW 会根据此标志决定是否尝试硬件解码）

    // 解码器和渲染器（参考 QMPlayer2 的架构）
    std::unique_ptr<Decoder> videoDecoder_;    // 视频解码器（FFDecSW 或 FFDecHW）

    // 渲染架构（VideoRenderer + VideoWidgetBase）
    std::shared_ptr<VideoRenderer> videoRenderer_; // 渲染器接口
    VideoWidgetBase *videoWidgetBase_ = nullptr;   // 视频窗口基类（不拥有所有权，由外部管理）

    // 线程管理（参考 QMPlayer2 的 PlayClass，使用智能指针管理生命周期）
    std::shared_ptr<DemuxerThread> demuxThread_;
    std::shared_ptr<VideoThread> videoThread_;
    std::shared_ptr<AudioThread> audioThread_;

    // 播放状态（使用状态机统一管理）
    PlaybackStateMachine stateMachine_;

    // 线程同步管理器
    ThreadSyncManager threadSyncManager_;

    // 流索引（从 Demuxer 获取）
    int videoStreamIndex_;
    int audioStreamIndex_;
    int subtitleStreamIndex_;

    // 时钟同步（使用 AVClock）
    std::unique_ptr<AVClock> masterClock_;  // 主时钟（替代旧的 clockbase_ 和 sync_）
    
    // TODO: 旧的时钟逻辑（保留用于兼容，AVClock完全集成后可删除）
    microseconds clockbase_{microseconds::min()};
    SyncMaster sync_{SyncMaster::Default};

    // 全局基准 PTS（用于音视频同步）
    // 在播放开始时设置，确保 VideoThread 和 AudioThread 使用相同的基准
    nanoseconds basePts_{kInvalidTimestamp};
    nanoseconds videoBasePts_{kInvalidTimestamp};
    mutable std::mutex basePtsMutex_; // 保护 basePts_ 的互斥锁（mutable 允许在 const 方法中使用）

    // TODO: 旧的视频时钟管理（保留用于兼容，AVClock完全集成后可删除）
    // 视频时钟基于最后渲染的帧的 PTS + 从渲染时间到现在的经过时间
    nanoseconds videoClock_{kInvalidTimestamp};
    std::chrono::steady_clock::time_point videoClockTime_{};  // 最后更新视频时钟的时间
    mutable std::mutex videoClockMutex_; // 保护 videoClock_ 的互斥锁（mutable 允许在 const 方法中使用）
    bool flushVideo_{false};             // 是否需要刷新视频解码器
    bool flushAudio_{false};             // 是否需要刷新音频解码器
    double videoSeekPos_{-1.0};          // 视频 seek 位置（秒），-1 表示不需要 seeking
    double audioSeekPos_{-1.0};          // 音频 seek 位置（秒），-1 表示不需要 seeking
    QWaitCondition emptyBufferCond_;     // 条件变量，用于唤醒线程

    // 动态同步阈值（用于优化音视频同步）
    nanoseconds syncThreshold_{milliseconds(50)};    // 正常阈值：50ms（增加）
    nanoseconds maxSyncThreshold_{milliseconds(500)}; // 最大阈值：500ms（进一步增加，避免过度跳帧）
    nanoseconds minSyncThreshold_{milliseconds(5)};  // 最小阈值：5ms

    // 同步误差统计
    nanoseconds avgSyncError_{0};
    int syncErrorCount_{0};

    // 线程健康检查时间戳（Video 和 Audio 需要）
    std::chrono::steady_clock::time_point lastVideoFrameTime_;
    std::chrono::steady_clock::time_point lastAudioFrameTime_;

    // Demuxer 健康检查时间戳（用于线程健康检查）
    std::chrono::steady_clock::time_point lastDemuxTime_;

    // 统计信息
    std::shared_ptr<videoDecoder::Statistics> statistics_;

    // 系统监控（可选，仅在调试/日志模式下启用）
    std::unique_ptr<SystemMonitor> systemMonitor_;
};
