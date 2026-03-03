#pragma once

#include "Demuxer.h"
#include "packet_queue.h"
#include "ffmpeg.h"
#include "GlobalDef.h"
#include "chronons.h"
#include "../ErrorRecoveryManager.h"

#include <QThread>
#include <QObject>
#include <atomic>
#include <mutex>
#include <chrono>
#include <thread>

class PlayController;  // Forward declaration

/**
 * DemuxerThread: 解复用线程
 * 负责从媒体文件读取数据包并分发到视频和音频队列
 * 
 * 职责：
 * - 循环读取数据包
 * - 根据 streamIndex 将数据包放入对应的 PacketQueue
 * - 处理 seek 请求
 * - 处理暂停状态
 * - 检测 EOF
 */
class DemuxerThread : public QThread
{
    Q_OBJECT

public:
    explicit DemuxerThread(PlayController* controller, QObject* parent = nullptr);
    ~DemuxerThread();

    // 设置 Demuxer（由 PlayController 在打开文件后调用）
    void setDemuxer(Demuxer* demuxer);
    
    // 设置流索引（由 PlayController 在打开文件后调用）
    void setStreamIndices(int videoIndex, int audioIndex, int subtitleIndex);
    
    // 设置 PacketQueue（由 PlayController 在打开文件后调用）
    // 直接使用具体的类型，因为 Video 和 Audio 的 PacketQueue 大小是固定的
    void setVideoPacketQueue(PacketQueue<50*1024*1024>* vPackets);
    void setAudioPacketQueue(PacketQueue<4*1024*1024>* aPackets);
    
    // 请求 Seek（由 PlayController 调用）
    // backward: true 表示向后搜索到关键帧，false 表示向前搜索
    // 注意：seeking 状态由 PlayController 的状态机管理，此方法只设置 seek 参数并清空队列
    void requestSeek(int64_t positionUs, bool backward = true);
    
    // 注意：requestStop() 和 setPaused() 已删除
    // 停止状态和暂停状态统一由 PlayController 的状态机管理
    // 线程会在循环中检查状态机状态并自动退出或暂停
    
    // 获取线程健康检查时间戳（由 PlayController 调用）
    std::chrono::steady_clock::time_point getLastDemuxTime() const { return lastDemuxTime_; }
    
    // 检查是否正在运行（使用 QThread::isRunning()）
    // bool isRunning() const { return !quit_.load(); }  // 使用 QThread 的 isRunning() 方法

signals:
    void seekFinished(int64_t positionUs);
    void eofReached();
    void errorOccurred(int errorCode);

protected:
    void run() override;

private:
    // 辅助方法：简化 run() 的实现
    void handleSeek();  // 处理 Seek 请求
    bool readAndDistributePacket();  // 读取并分发数据包
    void distributeVideoPacket(AVPacket* packet, bool isSeeking);  // 分发视频数据包
    void distributeAudioPacket(AVPacket* packet, bool isSeeking);  // 分发音频数据包

private:
    PlayController* controller_;
    Demuxer* demuxer_;
    
    // PacketQueue 指针（使用具体类型）
    PacketQueue<50*1024*1024>* vPackets_;
    PacketQueue<4*1024*1024>* aPackets_;
    
    // 流索引
    int videoStreamIndex_{-1};
    int audioStreamIndex_{-1};
    int subtitleStreamIndex_{-1};
    
    // 控制变量
    // 注意：所有状态（quit_、seeking_、paused_）已统一到 PlayController 的状态机
    
    // Seek 相关
    std::mutex seekMutex_;
    int64_t seekPositionUs_{0};
    bool seekBackward_{true};  // 是否向后搜索到关键帧
    bool seekInProgress_{false};  // 防止重复执行 seek 操作
    
    // 线程健康检查
    std::chrono::steady_clock::time_point lastDemuxTime_;
    
    // 错误恢复管理器
    ErrorRecoveryManager errorRecoveryManager_;
};
