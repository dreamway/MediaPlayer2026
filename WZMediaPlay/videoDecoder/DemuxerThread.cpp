#include "DemuxerThread.h"
#include "PlayController.h"

#include "videoDecoder/OpenALAudio.h"

#include <chrono>
#include <spdlog/spdlog.h>
#include <thread>
#include <QDebug>
#include <QThread>

extern spdlog::logger *logger;

DemuxerThread::DemuxerThread(PlayController *controller, QObject *parent)
    : QThread(parent)
    , controller_(controller)
    , demuxer_(nullptr)
    , vPackets_(nullptr)
    , aPackets_(nullptr)
    , videoStreamIndex_(-1)
    , audioStreamIndex_(-1)
    , subtitleStreamIndex_(-1)
    , seekPositionUs_(0)
    , lastDemuxTime_(std::chrono::steady_clock::now())
{
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread created");
}

DemuxerThread::~DemuxerThread()
{
    // 注意：requestStop() 已删除，停止状态统一由 PlayController 的状态机管理
    // 如果线程还在运行，通过 PlayController 的 stop() 方法来停止
    if (controller_) {
        controller_->stop();
    }

    // 检查线程是否还在运行（添加异常保护）
    bool wasRunning = false;
    try {
        wasRunning = isRunning();
    } catch (...) {
        logger->warn("DemuxerThread::~DemuxerThread: Exception while checking isRunning(), thread may be destroyed");
        wasRunning = false; // 假设已停止，继续清理
    }

    if (wasRunning) {
        wait(5000); // 等待最多5秒

        // 再次检查（添加异常保护）
        bool stillRunning = false;
        try {
            stillRunning = isRunning();
        } catch (...) {
            logger->warn("DemuxerThread::~DemuxerThread: Exception while checking isRunning() after wait");
            stillRunning = false;
        }

        if (stillRunning) {
            logger->warn("DemuxerThread still running after wait, terminating");
            terminate();
            wait(1000);
        }
    }
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread destroyed");
}

void DemuxerThread::setDemuxer(Demuxer *demuxer)
{
    demuxer_ = demuxer;
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::setDemuxer called");
}

void DemuxerThread::setStreamIndices(int videoIndex, int audioIndex, int subtitleIndex)
{
    videoStreamIndex_ = videoIndex;
    audioStreamIndex_ = audioIndex;
    subtitleStreamIndex_ = subtitleIndex;
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::setStreamIndices: video={}, audio={}, subtitle={}", videoIndex, audioIndex, subtitleIndex);
}

void DemuxerThread::setVideoPacketQueue(PacketQueue<50 * 1024 * 1024> *vPackets)
{
    vPackets_ = vPackets;
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::setVideoPacketQueue called");
}

void DemuxerThread::setAudioPacketQueue(PacketQueue<4 * 1024 * 1024> *aPackets)
{
    aPackets_ = aPackets;
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::setAudioPacketQueue called");
}

void DemuxerThread::requestSeek(int64_t positionUs, bool backward)
{
    {
        if (!controller_ || !controller_->getThreadSyncManager().tryLock(seekMutex_, std::chrono::milliseconds(100))) {
            logger->warn("DemuxerThread::requestSeek: Failed to lock seekMutex_ (timeout)");
        } else {
            seekPositionUs_ = positionUs;
            seekBackward_ = backward;
            seekInProgress_ = false; // 重置标志，允许新的 seek 操作
            controller_->getThreadSyncManager().unlock(seekMutex_);
        }
        // 注意：seeking 状态由 PlayController 的状态机管理，这里只设置请求参数
    }

    // 注意：队列清空由 PlayController::seek() 在锁定 VideoThread/AudioThread 后执行
    // 此处不调用 Reset()，避免竞态：VideoThread/AudioThread 可能正持有 packet 引用进行解码，
    // 若此时 Reset 会 av_packet_unref 导致 use-after-free 崩溃（BUG-001 Seek 崩溃根因）
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::requestSeek: {} us, backward: {} (queue clear deferred to seek() under lock)", positionUs, backward);
}

// 注意：requestStop() 和 setPaused() 已删除
// 停止状态和暂停状态统一由 PlayController 的状态机管理
// 线程会在循环中检查 controller_->isStopping()/isStopped() 和 isPaused() 并自动响应

// 辅助方法：处理 Seek 请求
void DemuxerThread::handleSeek()
{
    // 防止重复执行 seek 操作
    if (!controller_) {
        logger->warn("DemuxerThread::handleSeek: controller_ is null");
        return;
    }
    
    {
        if (!controller_ || !controller_->getThreadSyncManager().tryLock(seekMutex_, std::chrono::milliseconds(100))) {
            logger->warn("DemuxerThread::handleSeek: Failed to lock seekMutex_ (timeout)");
        } else {
            if (seekInProgress_) {
                SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: Seek already in progress, skipping duplicate request");
                controller_->getThreadSyncManager().unlock(seekMutex_);
                return;
            }
            seekInProgress_ = true;
            controller_->getThreadSyncManager().unlock(seekMutex_);
        }
    }

    int64_t seek_target = 0;
    bool backward = true;
    {
        if (!controller_ || !controller_->getThreadSyncManager().tryLock(seekMutex_, std::chrono::milliseconds(100))) {
            logger->warn("DemuxerThread::handleSeek: Failed to lock seekMutex_ (timeout)");
        } else {
            seek_target = seekPositionUs_;
            backward = seekBackward_;
            controller_->getThreadSyncManager().unlock(seekMutex_);
        }
    }

    // 验证seeking目标是否有效（避免seek到0导致UI错误）
    if (seek_target == 0) {
        SPDLOG_LOGGER_WARN(logger,"DemuxerThread::handleSeek: Ignoring seek request with target=0 (likely intermediate seek operation)");
        // 重置标志，允许后续seeking
        {
            if (!controller_ || !controller_->getThreadSyncManager().tryLock(seekMutex_, std::chrono::milliseconds(100))) {
                logger->warn("DemuxerThread::handleSeek: Failed to lock seekMutex_ (timeout)");
            } else {
                seekInProgress_ = false;
                controller_->getThreadSyncManager().unlock(seekMutex_);
            }
        }
        return;
    }

    SPDLOG_LOGGER_INFO(logger,"DemuxerThread: Seeking to {} us, backward: {}", seek_target, backward);

    // 执行 seek 操作
    int64_t seekPosMs = seek_target / 1000;
    bool seekSuccess = demuxer_ && demuxer_->isOpened() && demuxer_->seek(seekPosMs, backward);

    if (!seekSuccess) {
        logger->error("Seek failed via Demuxer");
        {
            if (!controller_ || !controller_->getThreadSyncManager().tryLock(seekMutex_, std::chrono::milliseconds(100))) {
                logger->warn("DemuxerThread::handleSeek: Failed to lock seekMutex_ (timeout)");
            } else {
                seekInProgress_ = false;
                controller_->getThreadSyncManager().unlock(seekMutex_);
            }
        }
        emit errorOccurred(-1);
        emit seekFinished(-1);
        return;
    }

    // 确保队列清空（防御性编程，requestSeek 时已清空）
    try {
        if (vPackets_ && vPackets_->Size() > 0) {
            vPackets_->Reset("VideoQueue");
        }
        if (aPackets_ && aPackets_->Size() > 0) {
            aPackets_->Reset("AudioQueue");
        }
    } catch (const std::exception &e) {
        logger->error("DemuxerThread: Exception while resetting queues: {}", e.what());
    }

    // 通知 PlayController seek 完成（状态转换由 PlayController 处理）
    emit seekFinished(seek_target);
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread: Seek finished, target: {} us", seek_target);

    // 等待PlayController的状态转换完成（避免竞态条件导致重复seek）
    // Qt信号是异步的，需要等待状态机从Seeking转换到Playing
    int waitCount = 0;
    const int maxWaitMs = 100;
    while (controller_ && controller_->isSeeking() && waitCount < maxWaitMs) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        waitCount++;
    }
    if (waitCount >= maxWaitMs && controller_ && controller_->isSeeking()) {
        SPDLOG_LOGGER_WARN(logger,"DemuxerThread::handleSeek: Timeout waiting for state transition ({}ms), but continuing", maxWaitMs);
    }

    // 立即重置标志，允许后续循环正常处理（避免重复跳过）
    // 注意：状态转换是异步的，可能在状态转换完成前循环会继续，所以需要立即重置
    {
        if (!controller_ || !controller_->getThreadSyncManager().tryLock(seekMutex_, std::chrono::milliseconds(100))) {
            logger->warn("DemuxerThread::handleSeek: Failed to lock seekMutex_ (timeout)");
        } else {
            seekInProgress_ = false;
            controller_->getThreadSyncManager().unlock(seekMutex_);
        }
    }
}

// 辅助方法：读取并分发数据包
bool DemuxerThread::readAndDistributePacket()
{
    if (!demuxer_ || !demuxer_->isOpened()) {
        logger->error("Demuxer not available for reading packet");
        emit errorOccurred(-1);
        return false; // 退出循环
    }

    AVPacket packet;
    bool packetRead = demuxer_->readPacket(packet);

    static int packetReadCount = 0;
    packetReadCount++;
    if (packetReadCount % 10000 == 1) {
        SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread::readAndDistributePacket: packetRead={}, isEof={}", packetRead, demuxer_->isEof());
    }

    if (!packetRead) {
        // 处理 EOF
        if (demuxer_->isEof()) {
            // 减少 EOF 日志输出频率，避免日志过多
            static int eofLogCount = 0;
            if (++eofLogCount % 1000000 == 1) { // 每1000000次输出一次
                SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread::readAndDistributePacket: EOF detected (count: {})", eofLogCount);
            }
            // 检查是否有 pending 的 seek 请求
            if (controller_ && controller_->isSeeking()) {
                SPDLOG_LOGGER_INFO(logger,"DemuxerThread: EOF detected, but seeking is pending");
                return true; // 继续循环处理 seek
            }

            // 检查队列是否为空
            if (vPackets_ && aPackets_ && vPackets_->IsEmpty() && aPackets_->IsEmpty()) {
                SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: EOF, all queues empty, emitting eofReached signal");
                emit eofReached();

                // 立即标记队列为 finished，确保 VideoThread 能检测到
                if (controller_ && !controller_->isSeeking()) {
                    if (vPackets_)
                        vPackets_->setFinished();
                    if (aPackets_)
                        aPackets_->setFinished();
                    SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: Queues marked as finished immediately after EOF");
                }

                // 等待一小段时间，看是否有 seek 请求
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (controller_ && controller_->isSeeking()) {
                    return true; // 有 seek 请求，继续循环
                }
                return false; // EOF 且没有 seek 请求，退出循环
            }
            // 队列还有数据，继续等待
            return true;
        } else {
            // 读取失败但不是 EOF，可能是其他错误
            logger->warn("Failed to read packet from Demuxer (not EOF)");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true; // 继续循环
        }
    }

    // 分发数据包到对应的队列
    bool isSeeking = controller_ && controller_->isSeeking();

    if (packet.stream_index == videoStreamIndex_) {
        distributeVideoPacket(&packet, isSeeking);
    } else if (packet.stream_index == audioStreamIndex_) {
        distributeAudioPacket(&packet, isSeeking);
    } else if (packet.stream_index == subtitleStreamIndex_) {
        SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: Subtitle packet ignored");
    }

    av_packet_unref(&packet);
    return true; // 继续循环
}

// 辅助方法：分发视频数据包
void DemuxerThread::distributeVideoPacket(AVPacket *packet, bool isSeeking)
{
    if (!vPackets_) {
        logger->error("DemuxerThread: vPackets_ is null");
        return;
    }

    if (vPackets_->put(packet, "VideoQueue")) {
        static int videoQueuePutCount = 0;
        videoQueuePutCount++;
        if (videoQueuePutCount % 100 == 1) {
            SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: VideoQueue put succeeded (count: {})", videoQueuePutCount);
        }
        return; // 成功放入队列
    }

    // 队列满的处理
    if (isSeeking) {
        // Seeking 期间：直接丢弃数据包（队列应该在 requestSeek 时已清空）
        SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: VideoQueue full during seeking, dropping packet");
        return;
    }

    // 使用 waitForSpace 进行线程同步，等待 VideoThread 消费队列
    size_t packetSize = packet->size;
    if (vPackets_->waitForSpace(packetSize, 5000)) { // 等待最多5秒
        if (vPackets_->put(packet, "VideoQueue")) {
            static int videoQueuePutCount = 0;
            videoQueuePutCount++;
            if (videoQueuePutCount % 100 == 1) {
                SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: VideoQueue put succeeded after waiting (count: {})", videoQueuePutCount);
            }
            return;
        }
    }

    // 等待超时，记录警告但继续（避免完全阻塞播放）
    logger->warn("DemuxerThread: VideoQueue full after waiting 5s, dropping packet (size: {})", packetSize);
}

// 辅助方法：分发音频数据包
void DemuxerThread::distributeAudioPacket(AVPacket *packet, bool isSeeking)
{
    if (!aPackets_) {
        logger->error("DemuxerThread: aPackets_ is null");
        return;
    }

    if (aPackets_->put(packet, "AudioQueue")) {
        return; // 成功放入队列
    }

    // 使用 waitForSpace 进行线程同步
    if (isSeeking) {
        SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: AudioQueue full during seeking, dropping packet");
        return;
    }

    size_t packetSize = packet->size;
    if (aPackets_->waitForSpace(packetSize, 2000)) { // 等待最多2秒
        if (aPackets_->put(packet, "AudioQueue")) {
            return;
        }
    }

    // 等待超时，记录警告但继续
    logger->warn("DemuxerThread: AudioQueue full after waiting 2s, dropping packet (size: {})", packetSize);
}

void DemuxerThread::run()
{
    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::run started");

    // 前置检查
    if (!demuxer_ || !demuxer_->isOpened()) {
        logger->error("DemuxerThread::run: Demuxer not available");
        emit errorOccurred(-1);
        return;
    }

    if (!vPackets_ || !aPackets_) {
        logger->error("DemuxerThread::run: PacketQueues not set");
        emit errorOccurred(-1);
        return;
    }

    try {
        // 主循环：统一使用 PlayController 的状态机检查退出条件
        SPDLOG_LOGGER_DEBUG(logger,
            "DemuxerThread::run: Entering main loop, controller_={}, isStopped()={}", (void *) controller_, controller_ ? controller_->isStopped() : true);
        while (controller_ && !controller_->isStopped()) {
            try {
                //SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread::run: Loop iteration");
                // 更新线程健康检查时间戳
                auto now = std::chrono::steady_clock::now();
                lastDemuxTime_ = now;
                if (controller_) {
                    controller_->getLastDemuxTime() = now;
                }

                // 处理 Seek 请求（状态由 PlayController 统一管理）
                if (controller_->isSeeking()) {
                    handleSeek();
                    continue; // Seek 处理完成后继续循环
                }

                // 处理暂停状态（状态由 PlayController 统一管理）
                if (controller_->isPaused()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    continue;
                }

                // 读取并分发数据包
                if (!readAndDistributePacket()) {
                    // readAndDistributePacket 返回 false 表示 EOF 或错误，退出循环
                    break;
                }
            } catch (const std::exception &e) {
                logger->error("DemuxerThread::run: Exception in main loop: {}", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (...) {
                logger->error("DemuxerThread::run: Unknown exception in main loop");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } catch (const std::exception &e) {
        logger->error("DemuxerThread::run: Fatal exception: {}", e.what());
        emit errorOccurred(-1);
    } catch (...) {
        logger->error("DemuxerThread::run: Fatal unknown exception");
        emit errorOccurred(-1);
    }

    // 清理：标记队列完成（只有在没有 seek 请求时）
    if (controller_ && !controller_->isSeeking()) {
        if (vPackets_)
            vPackets_->setFinished();
        if (aPackets_)
            aPackets_->setFinished();
        SPDLOG_LOGGER_DEBUG(logger,"DemuxerThread: Queues marked as finished");
    }

    SPDLOG_LOGGER_INFO(logger,"DemuxerThread::run finished");
}
