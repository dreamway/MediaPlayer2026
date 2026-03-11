#include "VideoThread.h"
#include "GlobalDef.h"
#include "PlayController.h"
#include "videoDecoder/AVClock.h"
#include "videoDecoder/Decoder.h"
#include "videoDecoder/Statistics.h"
#include "videoDecoder/VideoRenderer.h"
#include "videoDecoder/ffmpeg.h" // 包含 AVERROR 和 AVErrorEOF
#include "videoDecoder/opengl/DebugImageSaver.h"

#include <chrono>
#include <spdlog/spdlog.h>
#include <QCoreApplication>
#include <QSettings>

// 定义 AV_ERROR_MAX_STRING_SIZE（如果未定义）
#ifndef AV_ERROR_MAX_STRING_SIZE
#define AV_ERROR_MAX_STRING_SIZE 64
#endif

extern spdlog::logger *logger;

VideoThread::VideoThread(PlayController *controller, PacketQueue<50 * 1024 * 1024> *vPackets, QObject *parent)
    : AVThread(controller)
    , vPackets_(vPackets)
    , renderer_(nullptr)
    // 注意：paused_ 已统一到状态机管理
    , lastFpsTime_(std::chrono::steady_clock::now())
    , fpsFrameCount_(0)
    , currentFPS_(0.0f)
    , showFPS_(false)
{
    // 从配置文件读取 ShowFPS 设置
    QString configPath = QCoreApplication::applicationDirPath() + "/config/SystemConfig.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    QVariant variant = settings.value("/System/ShowFPS");
    showFPS_ = variant.isNull() ? false : variant.toBool();

    // 从配置文件读取是否启用调试图像保存（默认禁用，避免性能影响）
    QVariant debugImageVariant = settings.value("/Debug/SaveDebugImages");
    bool enableDebugImages = debugImageVariant.isNull() ? false : debugImageVariant.toBool();

    // 初始化调试图像保存器（只有在明确启用时才创建）
    if (enableDebugImages) {
        debugImageSaver_ = std::make_unique<DebugImageSaver>();
        SPDLOG_LOGGER_INFO(logger, "VideoThread: Debug image saving enabled");
    } else {
        SPDLOG_LOGGER_INFO(logger, "VideoThread: Debug image saving disabled (performance optimization)");
    }
    SPDLOG_LOGGER_INFO(logger, "VideoThread created (ShowFPS: {})", showFPS_);
}

VideoThread::~VideoThread()
{
    // 断开finished()信号连接，避免Qt自动删除对象
    // 析构函数中不应该再调用deleteLater
    disconnect(this, SIGNAL(finished()), this, SLOT(deleteLater()));

    // 停止调试图像保存器
    if (debugImageSaver_) {
        debugImageSaver_->stop();
        debugImageSaver_.reset();
    }

    requestStop();

    // 检查线程是否还在运行（添加异常保护）
    bool wasRunning = false;
    try {
        wasRunning = isRunning();
    } catch (...) {
        SPDLOG_LOGGER_WARN(logger, "VideoThread::~VideoThread: Exception while checking isRunning(), thread may be destroyed");
        wasRunning = false; // 假设已停止，继续清理
    }

    if (wasRunning) {
        wait(5000);

        // 再次检查（添加异常保护）
        bool stillRunning = false;
        try {
            stillRunning = isRunning();
        } catch (...) {
            SPDLOG_LOGGER_WARN(logger, "VideoThread::~VideoThread: Exception while checking isRunning() after wait");
            stillRunning = false;
        }

        if (stillRunning) {
            SPDLOG_LOGGER_WARN(logger, "VideoThread still running after wait, terminating");
            AVThread::terminate();
            wait(1000);
        }
    }
    SPDLOG_LOGGER_INFO(logger, "VideoThread destroyed");
}

void VideoThread::setDec(Decoder *dec)
{
    AVThread::setDec(dec);
    if (dec_) {
        SPDLOG_LOGGER_INFO(logger, "VideoThread::setDec: decoder set, type: {}, ptr: {}", dec_->name(), (void *) dec_);
    } else {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::setDec: decoder is null!");
    }
}

void VideoThread::setVideoRenderer(VideoRenderer *renderer)
{
    renderer_ = renderer;
    SPDLOG_LOGGER_INFO(logger, "VideoThread::setVideoRenderer: renderer set, type: {}", renderer ? renderer->name().toStdString() : "null");
}

void VideoThread::setStatistics(videoDecoder::Statistics *stats)
{
    statistics_ = stats;
    SPDLOG_LOGGER_INFO(logger, "VideoThread::setStatistics: statistics object set");
}

void VideoThread::requestStop()
{
    br_ = true; // 使用 AVThread 的 br_ 标志
    SPDLOG_LOGGER_INFO(logger, "VideoThread::requestStop called");
}

// 辅助方法实现
void VideoThread::handleFlushVideo()
{
    if (controller_->getFlushVideo()) {
        if (dec_) {
            dec_->flushBuffers();
            logger->debug("VideoThread::handleFlushVideo: Flushed decoder");
        }
        controller_->setFlushVideo(false);
    }
}

void VideoThread::handlePausedState()
{
    if (controller_ && controller_->isPaused()) {
        // 使用可中断的等待
        for (int i = 0; i < 10 && !br_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
}

bool VideoThread::handleSeekingState(bool &wasSeekingBefore)
{
    bool isSeeking = controller_->isSeeking();

    // 更新 seeking 状态跟踪
    if (isSeeking) {
        // 检测 seeking 状态变化：如果之前不是 seeking，现在是 seeking，说明刚开始 seeking
        if (!wasSeekingBefore) {
            wasSeekingBefore = true;

            if (!dec_) {
                SPDLOG_LOGGER_WARN(logger, "VideoThread::handleSeekingState: dec_ is null, skipping pre-decode");
                return true;
            }

            // Seeking预加载：预解码目标位置附近的多个关键帧（减少卡顿）
            AVPixelFormat dummyPixFmt = AV_PIX_FMT_NONE;
            Frame dummyFrame;
            int keyFrameCount = 0;

            try {
                while (keyFrameCount < 3 && !br_) { // 预解码3个关键帧
                    int ret = dec_->decodeVideo(nullptr, dummyFrame, dummyPixFmt, true, 0);
                    if (ret < 0 || dummyFrame.isEmpty()) {
                        break;
                    }

                    // 检查是否是关键帧
                    if (dummyFrame.avFrame() && (dummyFrame.avFrame()->flags & AV_FRAME_FLAG_KEY)) {
                        keyFrameCount++;
                    }

                    dummyFrame.clear();
                }
            } catch (const std::exception &e) {
                SPDLOG_LOGGER_ERROR(logger, "VideoThread::handleSeekingState: Exception while pre-decoding: {}", e.what());
            }

            SPDLOG_LOGGER_DEBUG(logger, "VideoThread::handleSeekingState: Pre-decoded {} keyframes", keyFrameCount);

            // 刷新解码器，丢弃所有待解码的帧
            dec_->flushBuffers();

            if (logger) {
                logger->debug("VideoThread::handleSeekingState: Flushed decoder at seeking start");
            }

            controller_->updateVideoClock(kInvalidTimestamp);
        }

        // 在 seeking 期间，消费队列中的旧数据包（防止队列满导致死锁）
        if (!vPackets_) {
            SPDLOG_LOGGER_ERROR(logger, "VideoThread::handleSeekingState: vPackets_ is null during seeking");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true; // 继续循环
        }

        size_t queueSize = vPackets_->Size();
        if (queueSize == 0) {
            // 队列已被 Reset，等待新数据
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            return true; // 继续循环
        }

        // 限制跳过数据包的速度
        const int maxSkipPerLoop = 10;
        int skippedCount = 0;
        while (skippedCount < maxSkipPerLoop) {
            const AVPacket *packet = vPackets_->peekPacket("VideoQueue");
            if (packet) {
                vPackets_->popPacket("VideoQueue");
                skippedCount++;

                size_t currentQueueSize = vPackets_->Size();
                if (currentQueueSize == 0) {
                    break;
                }
            } else {
                break;
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return true; // 继续循环
    }

    // Seeking 结束，重置标志并准备处理新数据
    if (wasSeekingBefore && !isSeeking) {
        wasSeekingBefore = false;
        wasSeekingRecently_ = true;
        isFirstFrameAfterSeek_ = true;  // 标记 seeking 后的第一帧，强制渲染
        videoBasePtsSet = false;  // 重置视频基准 PTS，让 Seek 后第一帧重新建立基准
        seekingStartTime_ = std::chrono::steady_clock::now(); // 记录seeking开始时间，用于超时保护
        eofAfterSeekCount_ = 0;

        if (dec_) {
            dec_->flushBuffers();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // 检查队列大小，如果队列很大，立即清空一部分
        size_t currentQueueSize = vPackets_->Size();
        if (currentQueueSize > 100) {
            int cleared = 0;
            while (vPackets_->Size() > 50 && cleared < 100) {
                const AVPacket *packet = vPackets_->peekPacket("VideoQueue");
                if (!packet) {
                    break;
                }
                vPackets_->popPacket("VideoQueue");
                cleared++;
            }
        }

        controller_->updateVideoClock(kInvalidTimestamp);
    }

    return false; // 不需要 continue，继续正常流程
}

bool VideoThread::decodeFrame(Frame &videoFrame, int &bytesConsumed, AVPixelFormat &newPixFmt, bool &isKeyFrame)
{
    const AVPacket *packet = vPackets_->peekPacket("VideoQueue");

    if (!packet) {
        // 记录队列状态以便调试
        bool isEmpty = vPackets_->IsEmpty();
        bool isFinished = vPackets_->IsFinished();
        size_t queueSize = vPackets_->Size();
        // 如果队列有大量数据但peekPacket返回nullptr，这是异常情况
        if (!isEmpty && queueSize > 10) {
            SPDLOG_LOGGER_WARN(
                logger,
                "VideoThread::decodeFrame: peekPacket returned nullptr but queue has {} packets! IsEmpty={}, IsFinished={}",
                queueSize,
                isEmpty,
                isFinished);
        } else {
            logger->debug("VideoThread::decodeFrame: No packet available in queue, IsEmpty={}, IsFinished={}, Size={}", isEmpty, isFinished, queueSize);
        }
        return false; // 没有数据包
    }

    isKeyFrame = (packet->flags & AV_PKT_FLAG_KEY) != 0;

    // Seeking后，跳过非关键帧直到找到关键帧（带超时保护）
    if (wasSeekingRecently_ && !isKeyFrame) {
        // 检查是否超时（例如5秒后强制停止跳过）
        auto elapsed = std::chrono::steady_clock::now() - seekingStartTime_;
        auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();

        if (elapsedMs > 5000) { // 5秒超时
            wasSeekingRecently_ = false;
            SPDLOG_LOGGER_WARN(logger, "VideoThread::decodeFrame: Timeout waiting for keyframe after seeking ({}ms), stopping keyframe wait", elapsedMs);
        } else {
            SPDLOG_LOGGER_DEBUG(logger, "VideoThread::decodeFrame: Skipping non-keyframe after seeking (wasSeekingRecently_=true, elapsed={}ms)", elapsedMs);
            vPackets_->popPacket("VideoQueue");
            videoFrame.clear();
            return false; // 跳过，继续下一个数据包
        }
    }

    // 找到第一个关键帧后，清除seeking状态
    if (wasSeekingRecently_ && isKeyFrame) {
        wasSeekingRecently_ = false;
        SPDLOG_LOGGER_INFO(logger, "VideoThread::decodeFrame: Found first keyframe after seeking, clearing wasSeekingRecently_ state");
    }

    try {
        // 记录packet信息用于性能分析
        if (logger && packet) {
            logger->debug(
                "VideoThread::decodeFrame: Processing packet pts={}, size={}, flags={}, isKeyFrame={}", packet->pts, packet->size, packet->flags, isKeyFrame);
        }

        // 记录解码开始时间
        auto decodeVideoStart = std::chrono::steady_clock::now();

        // 注意：decodeVideo 返回的是错误码，0 表示成功，负数表示错误
        bytesConsumed = dec_->decodeVideo(packet, videoFrame, newPixFmt, false, 0);

        // 记录解码耗时
        auto decodeVideoEnd = std::chrono::steady_clock::now();
        auto decodeVideoDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decodeVideoEnd - decodeVideoStart).count();

        // 记录解码结果以便调试
        if (bytesConsumed < 0 && bytesConsumed != AVERROR(EAGAIN)) {
            SPDLOG_LOGGER_WARN(
                logger,
                "VideoThread::decodeFrame: decodeVideo failed after {}ms, error: {} (packet: stream_index={}, pts={}, flags={}, isKeyFrame={})",
                decodeVideoDuration,
                bytesConsumed,
                packet->stream_index,
                packet->pts,
                packet->flags,
                isKeyFrame);
        } else if (bytesConsumed >= 0) {
            if (videoFrame.isEmpty()) {
                logger->debug("VideoThread::decodeFrame: decodeVideo succeeded after {}ms, frame empty (B-frame or delayed frame)", decodeVideoDuration);
            } else {
                logger->debug(
                    "VideoThread::decodeFrame: decodeVideo succeeded after {}ms, frame pts={}ms, size={}x{}",
                    decodeVideoDuration,
                    std::chrono::duration_cast<std::chrono::milliseconds>(videoFrame.ts()).count(),
                    videoFrame.width(0),
                    videoFrame.height(0));

                // 成功解码：更新统计 + 重置错误计数
                if (statistics_) {
                    statistics_->incrementDecodedFrames(true);
                    statistics_->setVideoDecodeTime(static_cast<double>(decodeVideoDuration));
                }
                errorRecoveryManager_.resetErrorCount(ErrorType::DecodeError);
            }
        }
    } catch (const std::exception &e) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::decodeFrame: Exception in decodeVideo: {}", e.what());
        bytesConsumed = AVERROR(EINVAL);

        ErrorInfo error(ErrorType::DecodeError, "Exception in decodeVideo: " + std::string(e.what()), AVERROR(EINVAL));
        RecoveryResult recovery = errorRecoveryManager_.handleError(error);

        if (recovery.action == RecoveryAction::StopPlayback) {
            emit errorOccurred(-1);
            return false;
        }
        if (recovery.action == RecoveryAction::FlushAndRetry && dec_) {
            dec_->flushBuffers();
            SPDLOG_LOGGER_INFO(logger, "VideoThread: FlushAndRetry — decoder flushed (retry {})", recovery.retryCount);
        }

        vPackets_->popPacket("VideoQueue");
        videoFrame.clear();
        return false;
    } catch (...) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::decodeFrame: Unknown exception in decodeVideo");
        bytesConsumed = AVERROR(EINVAL);

        ErrorInfo error(ErrorType::UnknownError, "Unknown exception in decodeVideo", AVERROR(EINVAL));
        RecoveryResult recovery = errorRecoveryManager_.handleError(error);

        if (recovery.action == RecoveryAction::StopPlayback) {
            emit errorOccurred(-1);
            return false;
        }
        if (recovery.action == RecoveryAction::FlushAndRetry && dec_) {
            dec_->flushBuffers();
        }

        vPackets_->popPacket("VideoQueue");
        videoFrame.clear();
        return false;
    }

    // 解码成功，保存帧（如果有输出帧）
    // 注意：frameToQImage对4K图像非常耗时（800ms+），只在调试关键帧时启用
    if (bytesConsumed == 0 && !videoFrame.isEmpty()) {
        if (debugImageSaver_ && isKeyFrame) { // 只保存关键帧，避免性能影响
            QImage img = frameToQImage(videoFrame);
            if (!img.isNull()) {
                // 添加时间戳到文件名
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now;
#ifdef _MSC_VER
                localtime_s(&tm_now, &time_t_now);
#else
                localtime_r(&time_t_now, &tm_now);
#endif

                char timeStr[20];
                strftime(timeStr, sizeof(timeStr), "%y%m%d-%H%M%S", &tm_now);

                // 添加毫秒
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
                QString filename = QString("debug/decode_after_%1_%2_%3ms.png").arg(frameCount_).arg(timeStr).arg(static_cast<int>(ms.count()));

                debugImageSaver_->enqueueImage(img, filename);
            }
        }
    }

    return true; // 解码成功或需要处理
}

bool VideoThread::processDecodeResult(int bytesConsumed, Frame &videoFrame, bool isKeyFrame, bool &packetPopped)
{
    packetPopped = false;

    if (bytesConsumed == 0) {
        // 解码成功，有输出帧
        vPackets_->popPacket("VideoQueue");
        packetPopped = true;

        // 检查 videoSeekPos
        double videoSeekPos = controller_->getVideoSeekPos();
        if (videoSeekPos > 0.0 && !videoFrame.isEmpty() && isValidTimestamp(videoFrame.ts())) {
            double framePtsSeconds = videoFrame.ts().count() / 1e9;
            if (framePtsSeconds < videoSeekPos) {
                videoFrame.clear();
                return false; // 跳过这个帧
            } else {
                controller_->setVideoSeekPos(-1.0);
                if (wasSeekingRecently_ && isKeyFrame) {
                    wasSeekingRecently_ = false;
                }
            }
        } else if (wasSeekingRecently_ && isKeyFrame && !videoFrame.isEmpty()) {
            wasSeekingRecently_ = false;
        }

        return true; // 可以渲染
    } else if (bytesConsumed == 1) {
        // 特殊值：packet 已被消费，但还没有输出帧（B帧等情况）
        // 需要 pop packet 并继续发送下一个
        bool popped = vPackets_->popPacket("VideoQueue");
        packetPopped = popped;
        if (!popped) {
            SPDLOG_LOGGER_WARN(logger, "VideoThread::processDecodeResult: popPacket failed for bytesConsumed=1");
        }
        return false; // 继续循环，处理下一个 packet
    } else if (bytesConsumed == 2) {
        // 特殊值：有输出帧，但 packet 未被消费（解码器缓冲区满的情况）
        // 不 pop packet，但可以渲染这一帧
        // 检查 videoSeekPos
        double videoSeekPos = controller_->getVideoSeekPos();
        if (videoSeekPos > 0.0 && !videoFrame.isEmpty() && isValidTimestamp(videoFrame.ts())) {
            double framePtsSeconds = videoFrame.ts().count() / 1e9;
            if (framePtsSeconds < videoSeekPos) {
                videoFrame.clear();
                return false; // 跳过这个帧
            } else {
                controller_->setVideoSeekPos(-1.0);
                if (wasSeekingRecently_ && isKeyFrame) {
                    wasSeekingRecently_ = false;
                }
            }
        } else if (wasSeekingRecently_ && isKeyFrame && !videoFrame.isEmpty()) {
            wasSeekingRecently_ = false;
        }
        return true; // 可以渲染，但不 pop packet
    } else if (bytesConsumed == AVERROR(EAGAIN)) {
        // 解码器异常状态，flush 并重试
        logger->debug("VideoThread::processDecodeResult: EAGAIN - unexpected state, flushing decoder");
        dec_->flushBuffers();
        return false; // 继续循环
    } else if (bytesConsumed == AVErrorEOF) {
        // 处理 EOF（简化版本）
        const AVPacket *packet = vPackets_->peekPacket("VideoQueue");
        bool packetIsKeyFrame = packet && (packet->flags & AV_PKT_FLAG_KEY) != 0;

        if (wasSeekingRecently_) {
            eofAfterSeekCount_++;
            if (packetIsKeyFrame && eofAfterSeekCount_ <= 3) {
                dec_->flushBuffers();
                videoFrame.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return false; // 继续循环
            } else if (!packetIsKeyFrame) {
                vPackets_->popPacket("VideoQueue");
                videoFrame.clear();
                return false; // 继续循环
            }
        } else {
            if (packetIsKeyFrame) {
                dec_->flushBuffers();
                videoFrame.clear();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return false;
            } else {
                vPackets_->popPacket("VideoQueue");
                videoFrame.clear();
                return false;
            }
        }
    } else {
        // 其他错误
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(bytesConsumed, errbuf, AV_ERROR_MAX_STRING_SIZE);
        SPDLOG_LOGGER_WARN(logger, "VideoThread::processDecodeResult: decodeVideo returned error: {} ({})", bytesConsumed, errbuf);

        vPackets_->popPacket();
        videoFrame.clear();
        return false;
    }

    return false;
}

// 辅助函数：将Frame转换为QImage（用于调试）
QImage VideoThread::frameToQImage(const Frame &videoFrame)
{
    if (videoFrame.isEmpty() || !videoFrame.hasCPUAccess()) {
        return QImage();
    }

    // 获取帧信息
    int width = videoFrame.width(0);
    int height = videoFrame.height(0);
    AVPixelFormat pixFmt = videoFrame.pixelFormat();

    // 只处理YUV420P格式，这是VideoRenderer支持的主要格式
    if (pixFmt != AV_PIX_FMT_YUV420P) {
        return QImage();
    }

    // 创建RGB32 QImage
    QImage image(width, height, QImage::Format_RGB32);
    if (image.isNull()) {
        return QImage();
    }

    // 获取YUV数据指针
    const uint8_t *yData = videoFrame.constData(0);
    const uint8_t *uData = videoFrame.constData(1);
    const uint8_t *vData = videoFrame.constData(2);
    int yStride = videoFrame.linesize(0);
    int uStride = videoFrame.linesize(1);
    int vStride = videoFrame.linesize(2);

    // 手动YUV到RGB转换
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // 计算YUV索引
            int yIdx = y * yStride + x;
            int uIdx = (y / 2) * uStride + (x / 2);
            int vIdx = (y / 2) * vStride + (x / 2);

            // 获取YUV值
            int Y = yData[yIdx];
            int U = uData[uIdx] - 128;
            int V = vData[vIdx] - 128;

            // 转换为RGB
            int R = Y + 1.402 * V;
            int G = Y - 0.34414 * U - 0.71414 * V;
            int B = Y + 1.772 * U;

            // 裁剪到0-255范围
            R = qBound(0, R, 255);
            G = qBound(0, G, 255);
            B = qBound(0, B, 255);

            // 设置像素值
            image.setPixelColor(x, y, QColor(R, G, B));
        }
    }

    return image;
}

bool VideoThread::renderFrame(Frame &videoFrame, int &frames)
{
    if (videoFrame.isEmpty()) {
        return false;
    }

#if 0
    // 渲染前保存帧（只在关键帧或特定帧数时保存，避免性能影响）
    if (debugImageSaver_ && (frameCount_ % 100 == 0)) {  // 每100帧保存一次
        QImage img = frameToQImage(videoFrame);
        if (!img.isNull()) {
            // 获取当前时间作为时间戳
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_now;
            localtime_s(&tm_now, &time_t_now);

            char timeStr[20];
            strftime(timeStr, sizeof(timeStr), "%y%m%d-%H%M%S", &tm_now);

            // 添加毫秒
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
            QString filename = QString("debug/render_before_%1_%2_%3ms.png")
                .arg(frameCount_)
                .arg(timeStr)
                .arg(static_cast<int>(ms.count()));

            debugImageSaver_->enqueueImage(img, filename);
        }
    }
#endif

    // 检查 seeking 状态
    if (controller_->isSeeking()) {
        videoFrame.clear();
        return false;
    }

    // 如果刚刚完成 seeking，确保第一帧是有效的
    if (wasSeekingRecently_) {
        if (videoFrame.isEmpty() || videoFrame.width(0) <= 0 || videoFrame.height(0) <= 0) {
            videoFrame.clear();
            return false;
        }

        // 清空 OpenGL 图像
        if (renderer_) {
            try {
                renderer_->clear();
            } catch (...) {
                // 忽略异常
            }
        }

        wasSeekingRecently_ = false;
    }

    // 更新健康检查时间戳
    controller_->getLastVideoFrameTime() = std::chrono::steady_clock::now();

    // 获取视频帧时间戳
    nanoseconds framePts = videoFrame.ts();
    if (!isValidTimestamp(framePts)) {
        // 无效的PTS，跳过帧
        return false;
    }

    // 增加帧计数
    frameCount_++;

    // 参考旧版本：移除基于 FPS 的帧间隔控制，使用音视频同步来决定渲染时机
    // 这样可以避免视频滞后于音频的问题

    // 设置视频基准 PTS（使用第一帧的 PTS 作为基准）
    // 关键：确保视频和音频使用相同的基准（basePts），而不是分别设置
    if (!videoBasePtsSet) {
        // 同时设置videoBasePts和basePts，确保音频和视频使用相同的基准
        controller_->setVideoBasePts(framePts);
        // 如果basePts还未设置，使用视频第一帧的PTS作为全局基准
        nanoseconds currentBasePts = controller_->getBasePts();
        if (!isValidTimestamp(currentBasePts)) {
            controller_->setBasePts(framePts);
            SPDLOG_LOGGER_INFO(
                logger, "VideoThread::renderFrame: Set basePts to video first frame PTS: {}ms", std::chrono::duration_cast<milliseconds>(framePts).count());
        } else {
            // basePts已设置（可能由音频设置），使用它作为基准
            SPDLOG_LOGGER_INFO(
                logger,
                "VideoThread::renderFrame: basePts already set to {}ms, using it as videoBasePts",
                std::chrono::duration_cast<milliseconds>(currentBasePts).count());
            controller_->setVideoBasePts(currentBasePts);
        }
        videoBasePtsSet = true;
        SPDLOG_LOGGER_INFO(
            logger, "VideoThread::renderFrame: videoBasePts set to {}ms", std::chrono::duration_cast<milliseconds>(controller_->getVideoBasePts()).count());
    }

    // 调整视频帧 PTS，使其与音频基准对齐
    // 使用basePts作为全局基准，确保音频和视频时钟一致
    nanoseconds adjustedFramePts = framePts;
    nanoseconds basePts = controller_->getBasePts();
    nanoseconds videoBasePts = controller_->getVideoBasePts();

    // 优先使用basePts（全局基准），如果未设置则使用videoBasePts
    nanoseconds referencePts = isValidTimestamp(basePts) ? basePts : videoBasePts;

    if (isValidTimestamp(referencePts)) {
        // 视频时钟基准对齐：减去基准PTS，使其从 0 开始
        adjustedFramePts = framePts - referencePts;
        if (logger && (frameCount_ % 100 == 0)) { // 每100帧输出一次
            logger->debug(
                "VideoThread::renderFrame: framePts={}ms, basePts={}ms, videoBasePts={}ms, adjustedFramePts={}ms",
                std::chrono::duration_cast<milliseconds>(framePts).count(),
                isValidTimestamp(basePts) ? std::chrono::duration_cast<milliseconds>(basePts).count() : -1,
                isValidTimestamp(videoBasePts) ? std::chrono::duration_cast<milliseconds>(videoBasePts).count() : -1,
                std::chrono::duration_cast<milliseconds>(adjustedFramePts).count());
        }
    }

    // 音视频同步：优先使用 getMasterClock()（基于 OpenAL AL_SAMPLE_OFFSET 的实际播放位置）
    // BUG-002 修复：AVClock 的 AudioClock 使用 audioPts_（最后解码 PTS），与实际播放位置相差较大
    // 导致视频认为严重滞后而强制快速渲染，造成视频过快、音画不同步
    nanoseconds masterClock = controller_->getMasterClock();

    // Fallback: 若 getMasterClock 未准备好，使用 AVClock
    if (!isValidTimestamp(masterClock)) {
        AVClock *avClock = controller_->getAVClock();
        if (avClock && avClock->isActive()) {
            double clockValue = avClock->value();
            masterClock = nanoseconds{static_cast<long long>(clockValue * 1000000000.0)};
        }
    }
    if (!isValidTimestamp(masterClock)) {
        masterClock = controller_->getVideoClock();
        if (isValidTimestamp(masterClock) && isValidTimestamp(referencePts)) {
            // getVideoClock 返回相对 basePts 的偏移，需加上 basePts 得到绝对位置
            masterClock = referencePts + masterClock;
        }
        if (!isValidTimestamp(masterClock)) {
            masterClock = framePts; // 使用帧 PTS 作为临时时钟（已是绝对位置）
            if (logger) {
                logger->debug(
                    "VideoThread::renderFrame: Using framePts as fallback clock: {}ms",
                    std::chrono::duration_cast<milliseconds>(framePts).count());
            }
        }
    }

    // 统一使用绝对时间比较：framePts 与 masterClock 均为绝对 PTS
    nanoseconds diff = framePts - masterClock;

    // 记录同步误差并更新阈值
    controller_->updateSyncThreshold(diff);

    // 参考旧版本：如果视频滞后太多（>1秒），强制推进，避免卡顿
    const nanoseconds MAX_FRAME_LAG = nanoseconds{1LL * 1000000000}; // 1秒
    bool frameExpired = (diff < -MAX_FRAME_LAG);                     // 视频滞后超过1秒

    // 关键修复：seeking 后的第一帧强制渲染，不应用同步跳帧逻辑
    // 这解决了 seek 到非关键帧位置时，关键帧 PTS 超前于 seek 目标位置导致帧被跳过的问题
    if (isFirstFrameAfterSeek_) {
        isFirstFrameAfterSeek_ = false;  // 只对第一帧生效
        SPDLOG_LOGGER_INFO(
            logger,
            "VideoThread::renderFrame: First frame after seek, forcing render (framePts={}ms, masterClock={}ms, diff={}ms)",
            std::chrono::duration_cast<milliseconds>(framePts).count(),
            std::chrono::duration_cast<milliseconds>(masterClock).count(),
            std::chrono::duration_cast<milliseconds>(diff).count());
        // 强制渲染，不跳过
    } else if (frameExpired) {
        // 如果视频滞后太多，强制渲染，避免一直卡顿
        if (logger && (frameCount_ % 10 == 0)) { // 每10帧输出一次，避免日志过多（BUG-026：降为 debug 减噪）
            SPDLOG_LOGGER_DEBUG(
                logger,
                "VideoThread::renderFrame: Frame expired (lag={}ms), forcing render: framePts={}ms, masterClock={}ms",
                std::chrono::duration_cast<milliseconds>(-diff).count(),
                std::chrono::duration_cast<milliseconds>(framePts).count(),
                std::chrono::duration_cast<milliseconds>(masterClock).count());
        }
        // 强制渲染，不跳过
    } else if (diff > controller_->getMaxSyncThreshold() * 2) {
        // 极端情况：视频超前太多（超过 2 倍最大阈值），跳过帧
        consecutiveSkipCount_++;
        if (logger) {
            logger->debug(
                "VideoThread::renderFrame: Skipping frame due to extreme advance ({} ms > {} ms)",
                std::chrono::duration_cast<milliseconds>(diff).count(),
                std::chrono::duration_cast<milliseconds>(controller_->getMaxSyncThreshold() * 2).count());
        }

        // 保存为上一帧（避免黑屏）
        lastRenderedFrame_ = videoFrame;
        hasLastRenderedFrame_ = true;

        // 更新丢帧统计
        if (statistics_) {
            statistics_->incrementDroppedFrames();
        }

        return false;
    } else {
        // 正常情况：添加调试日志（每100帧输出一次）
        if (logger && (frameCount_ % 100 == 0)) {
            logger->debug(
                "VideoThread::renderFrame: Normal sync: framePts={}ms, masterClock={}ms, diff={}ms",
                std::chrono::duration_cast<milliseconds>(framePts).count(),
                std::chrono::duration_cast<milliseconds>(masterClock).count(),
                std::chrono::duration_cast<milliseconds>(diff).count());
        }
    }

    // 正常渲染（FPS 控制已经处理了同步调整）
    auto renderStart = std::chrono::steady_clock::now();
    if (renderer_) {
        if (!renderer_->render(videoFrame)) {
            if (logger) {
                logger->warn("VideoThread::renderFrame: renderer_->render() failed");
            }
            return false;
        }
    } else {
        if (logger) {
            logger->error("VideoThread::renderFrame: renderer_ is null");
        }
        return false;
    }
    auto renderEnd = std::chrono::steady_clock::now();
    auto renderDuration = std::chrono::duration_cast<std::chrono::milliseconds>(renderEnd - renderStart).count();

    controller_->updateVideoClock(adjustedFramePts); // 使用调整后的 PTS
    videoFrame.clear();
    frames++;
    fpsFrameCount_++;
    consecutiveSkipCount_ = 0; // 重置跳过计数

    // 更新渲染统计
    if (statistics_) {
        statistics_->incrementRenderedFrames();
        statistics_->setVideoRenderTime(static_cast<double>(renderDuration));
        statistics_->setVideoClock(std::chrono::duration_cast<std::chrono::milliseconds>(adjustedFramePts).count() / 1000.0);
        statistics_->calculateFPS();
    }

    // FPS 计算
    if (showFPS_) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime_).count();
        if (elapsed >= 1000) {
            if (elapsed > 0) {
                currentFPS_ = (fpsFrameCount_ * 1000.0f) / elapsed;
            }
            SPDLOG_LOGGER_INFO(logger, "Video FPS: {:.1f} (rendered {} frames in {} ms)", currentFPS_, fpsFrameCount_, elapsed);
            fpsFrameCount_ = 0;
            lastFpsTime_ = now;
        }
    }

    return true;
}

void VideoThread::run()
{
    SPDLOG_LOGGER_INFO(logger, "VideoThread::run started");
    br_ = false; // 使用 AVThread 的 br_ 标志

    // 在VideoThread::run开始处
    SPDLOG_LOGGER_INFO(logger, "VideoThread::run: dec_={}, controller_={}, vPackets_={}", (void *) dec_, (void *) controller_, (void *) vPackets_);

    if (!dec_ || !controller_ || !vPackets_) {
        logger->critical("VideoThread initialization failed: missing required components");
        return;
    }

    // 设置视频基准 PTS（将在第一帧渲染时设置）
    videoBasePtsSet = false;

    if (!dec_) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: Decoder is null");
        emit errorOccurred(-1);
        return;
    }

    if (!renderer_) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: VideoRenderer is null");
        emit errorOccurred(-1);
        return;
    }

    if (!vPackets_) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: Video PacketQueue is null");
        emit errorOccurred(-1);
        return;
    }

    // 参考 QMPlayer2 的 VideoThr::run() 实现
    Frame videoFrame;
    int frames = 0;

    // 重置 seeking 相关状态（每次线程启动时重置，避免视频切换时状态混乱）
    wasSeekingRecently_ = false;
    eofAfterSeekCount_ = 0;
    isFirstFrameAfterSeek_ = false;  // 重置 seeking 后第一帧标志

    // 重置跳帧优化相关状态
    lastRenderedFrame_.clear();
    hasLastRenderedFrame_ = false;

    // 重置 FPS 计算相关状态
    lastFpsTime_ = std::chrono::steady_clock::now();
    fpsFrameCount_ = 0;
    currentFPS_ = 0.0f;

    // 在启动时flush解码器，确保解码器状态正确（避免EOF状态导致第一个关键帧无法解码）
    if (dec_) {
        dec_->flushBuffers();
        if (logger) {
            logger->debug("VideoThread::run: Flushed decoder at startup to ensure clean state");
        }
    }

    // 用于跟踪 seeking 状态变化（每次线程启动时重置）
    bool wasSeekingBefore = false;

    try {
        int loopCount = 0;
        int noFrameCount = 0; // 连续没有输出帧的次数
        while (!br_) {        // 使用 AVThread 的 br_ 标志
            auto loopStartTime = std::chrono::steady_clock::now();
            try {
                loopCount++;
                // 调试阶段更频繁记录
                if (loopCount % 50 == 0) {
                    size_t queueSize = vPackets_ ? vPackets_->Size() : 0;
                    logger->debug(
                        "VideoThread::run: Loop iteration {}, queue size: {}, frames rendered: {}, noFrameCount: {}", loopCount, queueSize, frames, noFrameCount);
                }

                // 处理 flushVideo 标志
                handleFlushVideo();
                videoFrame.clear(); // flushVideo 后清空帧

                // 处理暂停状态
                handlePausedState();
                if (br_) {
                    SPDLOG_LOGGER_INFO(logger, "VideoThread::run: Stopping flag detected, exiting");
                    break; // 如果收到停止信号，立即退出
                }
                // 检查是否应该停止
                if (controller_ && (controller_->isStopping() || controller_->isStopped())) {
                    SPDLOG_LOGGER_INFO(
                        logger,
                        "VideoThread::run: Stopping flag detected, controller_->isStopping()={}, controller_->isStopped()={}",
                        controller_->isStopping(),
                        controller_->isStopped());
                    break;
                }
                if (controller_ && controller_->isPaused()) {
                    SPDLOG_LOGGER_INFO(logger, "VideoThread::run: Paused flag detected, continuing");
                    continue;
                }

                // 处理 seeking 状态
                if (handleSeekingState(wasSeekingBefore)) {
                    SPDLOG_LOGGER_INFO(logger, "VideoThread::run: Seeking flag detected, continuing");
                    continue; // seeking 期间，继续循环
                }

                // 解码视频帧
                AVPixelFormat newPixFmt = AV_PIX_FMT_NONE;
                int bytesConsumed = 0;
                bool isKeyFrame = false;
                bool packetPopped = false;

                // 尝试解码帧
                auto decodeStartTime = std::chrono::steady_clock::now();
                if (!decodeFrame(videoFrame, bytesConsumed, newPixFmt, isKeyFrame)) {
                    // 记录解码失败的时间（用于性能分析）
                    auto decodeFailTime = std::chrono::steady_clock::now();
                    auto decodeFailDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decodeFailTime - decodeStartTime).count();
                    if (logger && decodeFailDuration > 5) { // 解码假性阻塞超过5ms才记录
                        logger->debug("VideoThread::run: decodeFrame failed after {}ms", decodeFailDuration);
                    }
                    // 没有数据包或跳过，检查队列状态
                    bool isEmpty = vPackets_->IsEmpty();
                    bool isFinished = vPackets_->IsFinished();
                    size_t queueSize = vPackets_->Size();

                    // 如果队列有大量数据但decodeFrame失败，记录警告
                    // 在seeking期间降低warning频率（因为解码器需要时间找到第一个关键帧）
                    if (!isEmpty && queueSize > 10) {
                        if (wasSeekingRecently_) {
                            // seeking期间，每100次才输出一次warning
                            static int seekingWarningCount = 0;
                            if (++seekingWarningCount % 100 == 1) {
                                SPDLOG_LOGGER_WARN(
                                    logger,
                                    "VideoThread::run: Queue has {} packets but decodeFrame failed during seeking (may be waiting for keyframe)",
                                    queueSize);
                            }
                        } else {
                            SPDLOG_LOGGER_WARN(
                                logger, "VideoThread::run: Queue has {} packets but decodeFrame failed, may be decoder issue or blocking", queueSize);
                        }
                    } else {
                        logger->debug(
                            "VideoThread::run: decodeFrame failed, checking queue state: IsEmpty={}, IsFinished={}, Size={}, frames={}",
                            isEmpty,
                            isFinished,
                            queueSize,
                            frames);
                    }

                    if (isEmpty) {
                        // 如果队列为空且未finished，只在已经渲染了一些帧后才检查播放完成
                        // 这防止在播放开始时立即退出
                        if (frames > 0) {
                            bool audioFinished = (controller_ && controller_->isAudioFinished());
                            bool videoFinished = vPackets_->IsFinished();
                            logger->debug(
                                "VideoThread::run: Video queue is empty, finished={}, audioFinished={}, frames={}, checking playback completion",
                                videoFinished,
                                audioFinished,
                                frames);
                            // 检查是否播放完成（需要同时检查视频和音频队列）
                            if (videoFinished && audioFinished) {
                                // 队列已结束，音频也结束，确认播放完成
                                SPDLOG_LOGGER_INFO(
                                    logger, "VideoThread::run: Playback completed (video finished={} and audio finished={})", videoFinished, audioFinished);
                                emit playbackCompleted(); // 发送播放完成信号
                                break;
                            } else {
                                logger
                                    ->debug("VideoThread::run: Playback not completed yet (video finished={}, audio finished={})", videoFinished, audioFinished);
                            }
                        } else {
                            // 还没有渲染任何帧，继续等待（可能是播放刚开始或音频时钟未准备好）
                            logger->debug("VideoThread::run: Queue empty but no frames rendered yet (frames={}), continuing to wait", frames);
                        }
                        // 等待新数据
                        std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        continue;
                    } else if (vPackets_->IsFinished()) {
                        // 只有视频结束，等待音频
                        logger->debug("VideoThread::run: Video queue finished, waiting for audio");
                        // 尝试刷新解码器获取剩余帧
                        bytesConsumed = dec_->decodeVideo(nullptr, videoFrame, newPixFmt, true, 0);
                        if (bytesConsumed >= 0 && !videoFrame.isEmpty()) {
                            // 还有剩余帧，处理并直接渲染
                            if (!processDecodeResult(bytesConsumed, videoFrame, isKeyFrame, packetPopped)) {
                                continue;
                            }
                            if (!renderFrame(videoFrame, frames)) {
                                SPDLOG_LOGGER_WARN(logger, "VideoThread::run: Failed to render final frame");
                                continue;
                            }

                            // 视频结束时也保持帧率控制
                            static auto lastRenderTimeEOF = std::chrono::steady_clock::now();
                            auto nowEOF = std::chrono::steady_clock::now();
                            auto timeSinceLastRenderEOF = std::chrono::duration_cast<std::chrono::milliseconds>(nowEOF - lastRenderTimeEOF).count();

                            // 根据视频帧率计算目标间隔（毫秒）
                            double targetIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(targetFrameInterval_).count();

                            if (timeSinceLastRenderEOF < targetIntervalMs * 0.95) {
                                auto sleepTime = static_cast<long long>(targetIntervalMs - timeSinceLastRenderEOF);
                                if (sleepTime > 0) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                                }
                            }
                            lastRenderTimeEOF = std::chrono::steady_clock::now();
                        }
                        // 使用可中断的等待
                        for (int i = 0; i < 10 && !br_; ++i) {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                        if (br_) {
                            break;
                        }
                        continue;
                    } else {
                        // 队列不为空但 decodeFrame 失败，可能是并发问题或需要更多数据
                        // 尝试渲染上一帧来避免黑屏闪烁
                        if (renderer_ && renderer_->hasLastFrame()) {
                            renderer_->renderLastFrame();
                        }

                        // 短暂等待后重试
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                }

                // decodeFrame 成功，记录解码时间
                auto decodeEndTime = std::chrono::steady_clock::now();
                auto decodeDuration = std::chrono::duration_cast<std::chrono::milliseconds>(decodeEndTime - decodeStartTime).count();
                if (logger && decodeDuration > 1) { // 解码时间超过10ms才记录
                    logger->debug("VideoThread::run: decodeFrame succeeded after {}ms", decodeDuration);
                }

                // decodeFrame 成功，处理解码结果（包括 popPacket）
                if (!processDecodeResult(bytesConsumed, videoFrame, isKeyFrame, packetPopped)) {
                    noFrameCount++;
                    // bytesConsumed=1 表示 packet 已消费但没有输出帧（B帧等情况），这是正常的
                    // 只有真正的错误才需要警告
                    if (bytesConsumed < 0 && bytesConsumed != AVERROR(EAGAIN)) {
                        SPDLOG_LOGGER_WARN(logger, "VideoThread::run: processDecodeResult failed, bytesConsumed={}", bytesConsumed);
                    }
                    continue; // 继续循环
                }
                noFrameCount = 0; // 成功输出帧，重置计数

                // 直接渲染帧（使用非阻塞连接）
                nanoseconds framePts = videoFrame.ts();
                if (logger && (frameCount_ % 10 == 0)) { // 每10帧输出一次
                    logger->debug(
                        "VideoThread::run: Rendering frame pts={}ms, width={}, height={}, format={}",
                        std::chrono::duration_cast<std::chrono::milliseconds>(framePts).count(),
                        videoFrame.width(0),
                        videoFrame.height(0),
                        av_get_pix_fmt_name(static_cast<AVPixelFormat>(videoFrame.pixelFormat())));
                }

                if (!renderFrame(videoFrame, frames)) {
                    // renderFrame失败（可能是同步跳帧），记录但继续
                    static int renderFailCount = 0;
                    if (++renderFailCount % 100 == 0) {
                        SPDLOG_LOGGER_WARN(logger, "VideoThread::run: renderFrame failed {} times", renderFailCount);
                    }
                    continue;
                }

                // 添加帧率控制：根据视频实际帧率动态调整，避免消费太快
                static auto lastRenderTime = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                auto timeSinceLastRender = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRenderTime).count();

                // 根据视频帧率计算目标间隔（毫秒）
                double targetIntervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(targetFrameInterval_).count();

                if (timeSinceLastRender < targetIntervalMs * 0.95) { // 留5%余量
                    auto sleepTime = static_cast<long long>(targetIntervalMs - timeSinceLastRender);
                    if (sleepTime > 0) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                    }
                }
                lastRenderTime = std::chrono::steady_clock::now();

                // 检查解码器错误
                if (dec_ && dec_->hasCriticalError()) {
                    SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: decoder has critical error");
                    emit errorOccurred(-1);
                    break;
                }

            } // 关闭内层 try
            catch (const std::exception &e) {
                SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: Exception in main loop: {}", e.what());
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            } catch (...) {
                SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: Unknown exception in main loop");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            // 记录循环时间（用于性能分析）
            auto loopEndTime = std::chrono::steady_clock::now();
            auto loopDuration = std::chrono::duration_cast<std::chrono::milliseconds>(loopEndTime - loopStartTime).count();
            if (logger && loopCount % 100 == 0 && loopDuration > 5) {
                logger->debug("VideoThread::run: Loop {} took {}ms", loopCount, loopDuration);
            }

        } // 关闭 while 循环
    } catch (const std::exception &e) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: Fatal exception: {}", e.what());
        emit errorOccurred(-1);
    } catch (...) {
        SPDLOG_LOGGER_ERROR(logger, "VideoThread::run: Fatal unknown exception");
        emit errorOccurred(-1);
    }

    SPDLOG_LOGGER_INFO(logger, "VideoThread::run finished, total frames rendered: {}", frames);
}

void VideoThread::setVideoFrameRate(double fps)
{
    if (fps > 0.0 && fps <= 120.0) { // 合理范围检查
        videoFrameRate_ = fps;
        // 根据帧率计算目标帧间隔（纳秒）
        double frameIntervalMs = 1000.0 / fps; // 毫秒
        targetFrameInterval_ = nanoseconds{static_cast<long long>(frameIntervalMs * 1000000.0)};

        if (logger) {
            SPDLOG_LOGGER_INFO(logger, "VideoThread::setVideoFrameRate: Set to {:.1f} FPS, frame interval: {}ms", fps, frameIntervalMs);
        }
    } else {
        if (logger) {
            SPDLOG_LOGGER_WARN(logger, "VideoThread::setVideoFrameRate: Invalid FPS value: {}, using default 25 FPS", fps);
        }
        videoFrameRate_ = 25.0;
        targetFrameInterval_ = nanoseconds{40000000}; // 40ms for 25fps
    }
}
