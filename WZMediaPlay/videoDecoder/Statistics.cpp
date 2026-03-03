#include "Statistics.h"
#include <QElapsedTimer>
#include <QDateTime>

namespace videoDecoder {

Statistics::Statistics(QObject* parent)
    : QObject(parent)
    , lastFPSTime_(QDateTime::currentMSecsSinceEpoch())
{
}

Statistics::~Statistics() = default;

void Statistics::updateVideoStats(const VideoStats& stats)
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_ = stats;
    emit videoStatsUpdated(videoStats_);
    emit statsUpdated();
}

void Statistics::updateAudioStats(const AudioStats& stats)
{
    std::lock_guard<std::mutex> lock(mutex_);
    audioStats_ = stats;
    emit audioStatsUpdated(audioStats_);
    emit statsUpdated();
}

void Statistics::updateSyncStats(const SyncStats& stats)
{
    std::lock_guard<std::mutex> lock(mutex_);
    syncStats_ = stats;
    emit syncStatsUpdated(syncStats_);
    emit statsUpdated();
}

void Statistics::incrementDecodedFrames(bool isVideo)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (isVideo) {
        videoStats_.decodedFrames++;
        frameCount_++;
    } else {
        audioStats_.decodedFrames++;
    }
    emit statsUpdated();
}

void Statistics::incrementRenderedFrames()
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_.renderedFrames++;
    frameCount_++;  // 供 calculateFPS() 使用，使 FPS 反映实际渲染帧率（BUG 11）
    emit statsUpdated();
}

void Statistics::incrementDroppedFrames()
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_.droppedFrames++;
    emit statsUpdated();
}

void Statistics::incrementPlayedFrames()
{
    std::lock_guard<std::mutex> lock(mutex_);
    audioStats_.playedFrames++;
    emit statsUpdated();
}

void Statistics::incrementSyncErrors()
{
    std::lock_guard<std::mutex> lock(mutex_);
    syncStats_.syncErrors++;
    emit statsUpdated();
}

void Statistics::setVideoFPS(double fps)
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_.fps = fps;
    emit statsUpdated();
}

void Statistics::setVideoBitrate(int bitrate)
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_.bitrate = bitrate;
    emit statsUpdated();
}

void Statistics::setAudioBitrate(int bitrate)
{
    std::lock_guard<std::mutex> lock(mutex_);
    audioStats_.bitrate = bitrate;
    emit statsUpdated();
}

void Statistics::setVideoDecodeTime(double time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_.decodeTime = time;
    emit statsUpdated();
}

void Statistics::setVideoRenderTime(double time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_.renderTime = time;
    emit statsUpdated();
}

void Statistics::setAudioDecodeTime(double time)
{
    std::lock_guard<std::mutex> lock(mutex_);
    audioStats_.decodeTime = time;
    emit statsUpdated();
}

void Statistics::setVideoClock(double clock)
{
    std::lock_guard<std::mutex> lock(mutex_);
    syncStats_.videoClock = clock;
    syncStats_.syncDiff = (syncStats_.videoClock - syncStats_.audioClock) * 1000.0; // 转换为ms
    emit statsUpdated();
}

void Statistics::setAudioClock(double clock)
{
    std::lock_guard<std::mutex> lock(mutex_);
    syncStats_.audioClock = clock;
    syncStats_.syncDiff = (syncStats_.videoClock - syncStats_.audioClock) * 1000.0; // 转换为ms
    emit statsUpdated();
}

Statistics::VideoStats Statistics::videoStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return videoStats_;
}

Statistics::AudioStats Statistics::audioStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return audioStats_;
}

Statistics::SyncStats Statistics::syncStats() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return syncStats_;
}

void Statistics::reset()
{
    std::lock_guard<std::mutex> lock(mutex_);
    videoStats_ = VideoStats();
    audioStats_ = AudioStats();
    syncStats_ = SyncStats();
    frameCount_ = 0;
    lastFPSTime_ = QDateTime::currentMSecsSinceEpoch();
    emit statsUpdated();
}

void Statistics::calculateFPS()
{
    std::lock_guard<std::mutex> lock(mutex_);
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    qint64 elapsed = currentTime - lastFPSTime_;

    if (elapsed >= 1000) { // 每秒计算一次
        videoStats_.fps = frameCount_ * 1000.0 / elapsed;
        frameCount_ = 0;
        lastFPSTime_ = currentTime;
        emit statsUpdated();
    }
}

} // namespace videoDecoder