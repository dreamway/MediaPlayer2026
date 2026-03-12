#pragma once

#include "GlobalDef.h"
#include "chronons.h"
#include "Frame.h"
#include "packet_queue.h"
#include "AVThread.h"
#include "../ErrorRecoveryManager.h"

#include <QObject>
#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

class PlayController;  // Forward declaration
class Decoder;
class VideoRenderer;
class DebugImageSaver;
namespace videoDecoder { class Statistics; }

/**
 * VideoThread: 视频解码线程（参考 QMPlayer2 的 VideoThr）
 * 负责视频解码和渲染
 * 
 * 职责：
 * - 从 PacketQueue 获取视频数据包
 * - 调用 Decoder 解码视频帧
 * - 处理音视频同步
 * - 调用 VideoRenderer 渲染帧
 * 
 * 设计：
 * - 参考 QMPlayer2 的 VideoThr::run() 实现
 * - 直接使用 DemuxerThread 的 PacketQueue，不再依赖 Video 类
 * - 继承 AVThread 基类，统一管理 Decoder 和线程同步
 */
class VideoThread : public AVThread
{
    Q_OBJECT

public:
    explicit VideoThread(PlayController* controller, PacketQueue<50*1024*1024>* vPackets, QObject* parent = nullptr);
    ~VideoThread();

    // 设置解码器（由 PlayController 调用，重写以添加日志）
    void setDec(Decoder* dec) override;

    // 设置新的 VideoRenderer（由 PlayController 调用）
    void setVideoRenderer(VideoRenderer* renderer);

    // 设置统计信息对象
    void setStatistics(videoDecoder::Statistics* stats);

    // 请求停止（由 PlayController 调用）
    void requestStop();

    // 设置视频帧率（用于动态调整渲染间隔）
    void setVideoFrameRate(double fps);
    
    
    // 检查是否正在运行（添加安全检查）
    bool isRunning() const {
        try {
            return this && QThread::isRunning();
        } catch (...) {
            return false;  // 如果对象已销毁，返回false
        }
    }

signals:
    void frameReady();
    void errorOccurred(int errorCode);

protected:
    void run() override;

private:
    // 辅助方法：简化 run() 的实现
    void handleFlushVideo();  // 处理 flushVideo 标志
    void handlePausedState();  // 处理暂停状态
    bool handleSeekingState(bool& wasSeekingBefore);  // 处理 seeking 状态，返回是否应该继续循环
    bool decodeFrame(Frame& videoFrame, int& bytesConsumed, AVPixelFormat& newPixFmt, bool& isKeyFrame);  // 解码视频帧
    bool processDecodeResult(int bytesConsumed, Frame& videoFrame, bool isKeyFrame, bool& packetPopped);  // 处理解码结果
    bool renderFrame(Frame& videoFrame, int& frames);  // 渲染帧

signals:
    // 播放完成信号
    void playbackCompleted();

private:
    PacketQueue<50*1024*1024>* vPackets_;  // 视频数据包队列（由 DemuxerThread 提供）

    VideoRenderer* renderer_ = nullptr;  // 新渲染器（不拥有所有权）
    
    // Seeking 状态管理（改为成员变量，避免静态变量在视频切换时状态混乱）
    // 注意：这些是线程内部的优化标志，不是状态机的状态
    bool wasSeekingRecently_{false};  // 用于跟踪是否刚刚完成 seeking（用于跳过非关键帧优化）
    int eofAfterSeekCount_{0};  // Seeking 后的 EOF 计数（用于处理解码器状态异常）
    std::chrono::steady_clock::time_point seekingStartTime_;  // seeking开始时间，用于超时保护
    bool isFirstFrameAfterSeek_{false};  // seeking 后的第一帧，强制渲染（不应用同步跳帧逻辑）
    int framesAfterSeek_{0};  // seeking 后已渲染的帧数（用于 grace period）
    
    // FPS 计算相关
    std::chrono::steady_clock::time_point lastFpsTime_;
    int fpsFrameCount_{0};
    float currentFPS_{0.0f};
    bool showFPS_{false};  // 是否显示 FPS（从配置文件读取）
    int frameCount_{0};     // 总帧计数（用于调试输出）
    bool videoBasePtsSet = false;

    // 基于视频 FPS 的渲染控制
    std::chrono::steady_clock::time_point lastRenderTime_;  // 上次渲染时间
    nanoseconds lastFramePts_{kInvalidTimestamp};         // 上次渲染帧的 PTS
    nanoseconds targetFrameInterval_{16666666};             // 动态计算的目标帧间隔（基于视频 FPS）
    double videoFrameRate_{25.0};                           // 视频帧率，从Demuxer获取

    // 跳帧优化：保留上一帧，减少闪烁
    Frame lastRenderedFrame_;  // 上一帧（用于跳帧时避免黑屏）
    bool hasLastRenderedFrame_{false};  // 是否有上一帧
    int consecutiveSkipCount_{0};  // 连续跳过帧的计数（避免过度跳过导致卡顿）
    
    // 错误恢复管理器
    ErrorRecoveryManager errorRecoveryManager_;
    
    // 调试图像保存器（用于保存关键帧，辅助定位硬解码渲染问题）
    std::unique_ptr<DebugImageSaver> debugImageSaver_;
    QImage frameToQImage(const Frame &videoFrame);

    // 统计信息
    videoDecoder::Statistics* statistics_ = nullptr;
};
