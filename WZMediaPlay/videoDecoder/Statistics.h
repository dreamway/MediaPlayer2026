#pragma once

#include <QObject>
#include <mutex>

namespace videoDecoder {

class Statistics : public QObject {
    Q_OBJECT

public:
    struct VideoStats {
        int decodedFrames = 0;      // 解码帧数
        int renderedFrames = 0;     // 渲染帧数
        int droppedFrames = 0;      // 丢帧数
        double fps = 0.0;           // 当前帧率
        int bitrate = 0;            // 码率 (kbps)
        double decodeTime = 0.0;    // 解码时间（ms）
        double renderTime = 0.0;    // 渲染时间（ms）
    };

    struct AudioStats {
        int decodedFrames = 0;      // 解码帧数
        int playedFrames = 0;       // 播放帧数
        int bitrate = 0;            // 码率 (kbps)
        double decodeTime = 0.0;    // 解码时间（ms）
    };

    struct SyncStats {
        double videoClock = 0.0;    // 视频时钟
        double audioClock = 0.0;    // 音频时钟
        double syncDiff = 0.0;      // 同步差异（ms）
        int syncErrors = 0;         // 同步错误次数
    };

    explicit Statistics(QObject* parent = nullptr);
    ~Statistics() override;

    // 更新统计
    void updateVideoStats(const VideoStats& stats);
    void updateAudioStats(const AudioStats& stats);
    void updateSyncStats(const SyncStats& stats);

    // 增量更新
    void incrementDecodedFrames(bool isVideo);
    void incrementRenderedFrames();
    void incrementDroppedFrames();
    void incrementPlayedFrames();
    void incrementSyncErrors();

    // 设置性能指标
    void setVideoFPS(double fps);
    void setVideoBitrate(int bitrate);
    void setAudioBitrate(int bitrate);
    void setVideoDecodeTime(double time);
    void setVideoRenderTime(double time);
    void setAudioDecodeTime(double time);
    void setVideoClock(double clock);
    void setAudioClock(double clock);

    // 获取统计
    VideoStats videoStats() const;
    AudioStats audioStats() const;
    SyncStats syncStats() const;

    // 重置统计
    void reset();

    // 计算实时FPS
    void calculateFPS();

signals:
    void statsUpdated();  // 统计更新信号
    void videoStatsUpdated(const VideoStats& stats);
    void audioStatsUpdated(const AudioStats& stats);
    void syncStatsUpdated(const SyncStats& stats);

private:
    mutable std::mutex mutex_;
    VideoStats videoStats_;
    AudioStats audioStats_;
    SyncStats syncStats_;

    // FPS计算相关
    int frameCount_ = 0;
    qint64 lastFPSTime_ = 0;
};

} // namespace videoDecoder