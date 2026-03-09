#pragma once

#include "videoDecoder/audio.h"
#include "videoDecoder/video.h"
#include "videoDecoder/hardware_decoder.h"
#include "system_monitor.h"

#include "GlobalDef.h"
#include "videoDecoder/chronons.h"
#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <QObject>
#include <QString>

#define ERROR_LEN 1024
#define PRINT_LOG 1

typedef struct SeekInfo
{
    int64_t seek_pos;
    int64_t seek_rel;
    int seek_flags;
} SeekInfo;

/**
Movie:
视频文件总入口，含视频解码及音频解码，定位跳转等
*/
class Movie : public QObject
{
    Q_OBJECT

public:
    Movie();
    ~Movie();

    int Open(QString filename);

    void PlayPause(bool isPause);
    void Stop();

    bool IsOpened() const;
    bool IsPlaying() const;
    bool IsStopped();
    bool IsPaused() const;

    int64_t GetDurationInMs() const;

    void SetVolume(float volume);
    float GetVolume();
    void ToggleMute();

    bool SetSeek(int64_t position);
    void SetSeekFlag(bool flag);

    bool SetPlaybackRate(double rate);
    double GetPlaybackRate() const;
    float GetVideoFrameRate();

    std::pair<AVFrame *, int64_t> currentFrame();

    nanoseconds getMasterClock();
    nanoseconds getClock();

    PlayState GetPlayState();
    bool IsSeeking();
    
    // 线程健康检查
    bool isThreadHealthy();
    
    // 等待视频预加载完成
    void waitForVideoPreload(int timeoutMs = 5000);
    
    // 检测是否播放完成（综合多种条件）
    bool isPlaybackFinished(int64_t currentElapsedSeconds);
    
    // 检查并主动停止播放（如果检测到播放完成）
    // 返回true表示已触发停止，false表示未检测到播放完成
    bool checkAndStopIfFinished(int64_t currentElapsedSeconds);

signals:
    void playStateChanged(PlayState state);
    void seekingFinished(int64_t seek_target);

private:
    int open(QString filename);

    //核心解码线程
    int startDemux();
    int pause(bool isPause);

    int stop();

    void reset();
    int streamComponentOpen(unsigned int stream_index);
    void initHWDecoder(const AVCodec *codec);
    
    // 硬件解码器（仅用于视频）
    std::unique_ptr<HardwareDecoder> hardwareDecoder_;
    bool enableHardwareDecoding_;  // 是否启用硬件解码

    friend class Audio;
    friend class Video;

    Audio audio_;
    Video video_;

    AVFormatContextPtr fmtctx_;
    AVCodecContext *avCodecContext_;
    std::mutex lock;

    microseconds clockbase_{microseconds::min()};
    SyncMaster sync_{SyncMaster::Default};

    //原子变量，需使用原子操作来读取及 修改它的值
    std::atomic<bool> quit_{true};

    std::thread demuxThread_;
    std::thread audioThread_;
    std::thread videoThread_;

private:
    std::mutex mSeekInfoMutex;
    SeekInfo mSeekInfo;
    std::atomic<bool> mSeekFlag{false};

private:
    int64_t totalMilliseconds;
    int mVideoStreamIndex;
    int mAudioStreamIndex;
    int64_t mTotalFrameNum;

    int mSubtitleStreamIndex;
    AVStream *mVideoStream;
    AVStream *mAudioStream;
    AVStream *mSubtitleStream;

    int videoWidth_, videoHeight_;
    qreal m_frameRate = 0;

    // 获取视频参数，使用avcodec_parameters_free()清理空间
    virtual AVCodecParameters *getVideoParameters();

    // 获取音频参数，使用avcodec_parameters_free()清理空间
    virtual AVCodecParameters *getAudioParameters();

    void showError(int err);
    char *m_error = nullptr;

private:
    PlayState playState;
    void setPlayState(PlayState state);
    
    // 线程健康检查相关
    std::chrono::steady_clock::time_point lastVideoFrameTime_;
    std::chrono::steady_clock::time_point lastAudioFrameTime_;
    std::chrono::steady_clock::time_point lastDemuxTime_;
    
    // 系统资源监控
    std::unique_ptr<SystemMonitor> systemMonitor_;
};
