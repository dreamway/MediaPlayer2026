#pragma once

#include "ffmpeg.h"
#include "chronons.h"

#include <QString>
#include <mutex>
#include <atomic>

/**
 * Demuxer: 解复用器
 * 封装 AVFormatContext 相关操作，负责打开文件、读取流信息、读取数据包
 * 
 * 职责：
 * - 打开媒体文件
 * - 读取流信息
 * - 读取数据包
 * - Seek 操作
 */
class Demuxer
{
public:
    Demuxer();
    ~Demuxer();

    // 文件操作
    bool open(const QString& filename);
    void close();
    bool isOpened() const;

    // 流信息
    int getVideoStreamIndex() const { return videoStreamIndex_; }
    int getAudioStreamIndex() const { return audioStreamIndex_; }
    int getSubtitleStreamIndex() const { return subtitleStreamIndex_; }
    AVStream* getVideoStream() const { return videoStream_; }
    AVStream* getAudioStream() const { return audioStream_; }
    AVStream* getSubtitleStream() const { return subtitleStream_; }
    int64_t getDurationMs() const { return durationMs_; }

    // 视频流信息
    int getVideoWidth() const { return videoWidth_; }
    int getVideoHeight() const { return videoHeight_; }
    double getVideoFrameRate() const { return videoFrameRate_; }
    int64_t getTotalFrameNum() const { return totalFrameNum_; }

    // 数据包读取（线程安全）
    bool readPacket(AVPacket& packet);
    bool isEof() const { return eof_.load(); }

    // Seek 操作（线程安全）
    // backward: true 表示向后搜索到关键帧，false 表示向前搜索
    // 对于视频解码，通常应该使用 backward=true 来确保 seek 到关键帧
    bool seek(int64_t positionMs, bool backward = true);
    // bool isSeeking() const { return seeking_.load(); }

    // 获取 AVFormatContext（用于向后兼容，逐步移除）
    AVFormatContext* getFormatContext() const { return fmtctx_.get(); }
    
    // 释放 AVFormatContext 的所有权（用于向后兼容）
    // 调用后，Demuxer 不再管理 AVFormatContext 的生命周期
    AVFormatContext* releaseFormatContext();

private:
    AVFormatContextPtr fmtctx_;
    
    // 流索引
    int videoStreamIndex_{-1};
    int audioStreamIndex_{-1};
    int subtitleStreamIndex_{-1};
    
    // 流指针
    AVStream* videoStream_{nullptr};
    AVStream* audioStream_{nullptr};
    AVStream* subtitleStream_{nullptr};
    
    // 媒体信息
    int64_t durationMs_{0};
    int videoWidth_{0};
    int videoHeight_{0};
    double videoFrameRate_{0.0};
    int64_t totalFrameNum_{0};
    
    // 状态 - 数据流状态（不同于播放控制状态）
    std::atomic<bool> eof_{false};
    
    // 线程安全
    mutable std::mutex mutex_;
};
