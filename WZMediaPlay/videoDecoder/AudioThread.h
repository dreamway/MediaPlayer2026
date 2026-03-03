#pragma once

#include "../ErrorRecoveryManager.h"
#include "AVThread.h"
#include "GlobalDef.h"
#include "OpenALAudio.h"
#include "chronons.h"

#include <atomic>
#include <memory>
#include <QObject>

class PlayController; // Forward declaration
namespace videoDecoder { class Statistics; }

/**
 * AudioThread: 音频解码线程（重构后，参考 VideoThread 架构）
 * 负责音频解码和输出
 * 
 * 职责：
 * - 从 PacketQueue 获取音频数据包
 * - 解码音频数据（decodeFrame）
 * - 重采样音频数据（readAudio）
 * - 调用 Audio::writeAudio() 输出到音频设备
 * - 处理 seeking 状态（消费队列中的旧数据包）
 * - 处理暂停状态
 * - 更新音频时钟供视频同步使用
 * 
 * 设计：
 * - 参考 VideoThread 的架构，在 run() 中实现解码循环
 * - Audio 类只负责音频输出（类似 VideoWriter），不包含解码逻辑
 * - 所有 FFmpeg 解码相关的资源都在 AudioThread 中管理
 * - 继承 AVThread 基类，统一管理线程同步（注意：AudioThread 不使用 Decoder 接口，dec_ 为 nullptr）
 */
class AudioThread : public AVThread
{
    Q_OBJECT

public:
    explicit AudioThread(PlayController *controller, Audio *audio, QObject *parent = nullptr);
    ~AudioThread();

    // 设置解码器上下文（由 PlayController 调用）
    void setCodecContext(AVCodecContextPtr codecctx, AVStream *stream);

    // 请求停止（由 PlayController 调用）
    void requestStop();

    // 设置暂停状态（由 PlayController 调用）
    void setPaused(bool paused);

    // 检查是否正在运行
    bool isRunning() const { return QThread::isRunning(); }

    // 获取音频时钟（供 PlayController 调用，用于音视频同步）
    nanoseconds getClock() const;

    // 设置统计信息对象
    void setStatistics(videoDecoder::Statistics* stats);

signals:
    void errorOccurred(int errorCode);

protected:
    void run() override;

private:
    // 解码相关方法
    int decodeFrame();                                     // 解码一帧音频
    bool readAudio(uint8_t *samples, unsigned int length); // 读取解码后的音频数据

    // 初始化 FFmpeg 解码相关资源
    bool initializeDecoder();
    void cleanupDecoder();

    // 辅助方法（用于简化run()方法）
    void handleFlushAudio();                  // 处理flushAudio标志
    void handlePausedState();                 // 处理暂停状态
    bool handleSeekingState();                // 处理seeking状态（返回true表示应继续循环）
    bool decodeAndWriteAudio(int buffer_len); // 解码并写入音频数据
    bool checkPlaybackComplete();             // 检查播放是否完成

    Audio *audio_;                           // 不拥有所有权，由 PlayController 管理
    PacketQueue<4 * 1024 * 1024> *aPackets_; // 音频数据包队列（由 PlayController 管理）

    // FFmpeg 解码相关资源（从 Audio 迁移）
    AVStream *stream_{nullptr};
    AVCodecContextPtr codecctx_;
    AVFramePtr decodedFrame_;
    SwrContextPtr swrctx_;

    // 重采样相关
    uint64_t dstChanLayout_{0};
    AVSampleFormat dstSampleFmt_{AV_SAMPLE_FMT_NONE};
    unsigned int frameSize_{0}; // 每样本字节数（用于计算 buffer_len）
    uint8_t *samples_{nullptr};
    int samplesLen_{0};
    int samplesPos_{0};
    int samplesMax_{0};

    // 音频时钟（在 AudioThread 中管理）
    nanoseconds currentPts_{0};
    nanoseconds deviceStartTime_{nanoseconds::min()};

    // wasSeeking_ 用于跟踪 seeking 状态变化（检测从 false 到 true 的变化，执行一次性初始化）
    // 注意：这是线程内部的优化标志，不是状态机的状态
    bool wasSeeking_{false};

    // 用于记录 seeking 后第一个解码的音频帧的 PTS（用于验证）
    bool firstFrameAfterSeekLogged_{false};

    // 错误恢复管理器
    ErrorRecoveryManager errorRecoveryManager_;

    // 统计信息
    videoDecoder::Statistics* statistics_ = nullptr;

public:
    // 音频结束标志（供 PlayController 调用）
    // 注意：这是音频线程的业务状态，不是状态机的状态
    std::atomic<bool> audioFinished_{false};
};
