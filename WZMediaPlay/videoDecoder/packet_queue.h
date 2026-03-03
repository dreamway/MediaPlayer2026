#pragma once

#include "ffmpeg.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "../GlobalDef.h"
#include "spdlog/spdlog.h"
#include <QDebug>
#include <QLoggingCategory>

template<size_t SizeLimit>
class PacketQueue
{
public:
    PacketQueue() = default;
    ~PacketQueue()
    {
        for (AVPacket &pkt : packets_) {
            av_packet_unref(&pkt);
        }
        packets_.clear();
        totalsize_ = 0;
    }

    int sendTo(AVCodecContext *codecctx, const char *queueName = nullptr)
    {
        try {
            if (!codecctx) {
                logger->error("PacketQueue[{}]: sendTo called with null codecctx", queueName ? queueName : "Unknown");
                return AVERROR(EINVAL);
            }

            std::unique_lock<std::mutex> lck(mtx_);

            AVPacket *pkt = getPacket(lck);
            int ret = avcodec_send_packet(codecctx, pkt);

            if (!pkt) {
                if (!ret) {
                    return AVErrorEOF;
                }
                return ret;
            }

            if (ret != AVERROR(EAGAIN)) {
                pop();
            }

            return ret;
        } catch (const std::exception &e) {
            logger->error("PacketQueue[{}]: Exception in sendTo: {}", queueName ? queueName : "Unknown", e.what());
            return AVERROR(EINVAL);
        } catch (...) {
            logger->error("PacketQueue[{}]: Unknown exception in sendTo", queueName ? queueName : "Unknown");
            return AVERROR(EINVAL);
        }
    }

    void setFinished()
    {
        {
            std::lock_guard<std::mutex> lck(mtx_);
            finished_ = true;

            while (totalsize_ > 0) {
                pop();
            }
            totalsize_ = 0;
        }
        cond_.notify_one();
    }

    bool put(const AVPacket *pkt, const char *queueName = nullptr)
    {
        try {
            if (!pkt) {
                logger->warn("PacketQueue[{}]: put called with null packet", queueName ? queueName : "Unknown");
                return false;
            }

            {
                std::lock_guard<std::mutex> lck(mtx_);
                if (totalsize_ >= SizeLimit) {
                    return false;
                }
                size_t oldSize = packets_.size();
                packets_.push_back(AVPacket{});
                if (av_packet_ref(&packets_.back(), pkt) != 0) {
                    logger->warn("PacketQueue[{}]: av_packet_ref failed", queueName ? queueName : "Unknown");
                    packets_.pop_back();
                    return false; // 修复：应该返回 false，表示失败
                }
                totalsize_ += static_cast<size_t>(packets_.back().size);
                if (queueName && logger) {
                    // logger->debug("PacketQueue[{}]: put packet, size: {}->{}, totalSize: {}, packet: stream_index={}, pts={}, flags={}",
                    //     queueName, oldSize, packets_.size(), totalsize_,
                    //     pkt->stream_index, pkt->pts, pkt->flags);
                }
            }
            cond_.notify_one();
            return true;
        } catch (const std::exception &e) {
            logger->error("PacketQueue[{}]: Exception in put: {}", queueName ? queueName : "Unknown", e.what());
            return false;
        } catch (...) {
            logger->error("PacketQueue[{}]: Unknown exception in put", queueName ? queueName : "Unknown");
            return false;
        }
    }

    void logQueueState(const char *queueName)
    {
        std::lock_guard<std::mutex> lck(mtx_);
        size_t queueSize = packets_.size();
        if (queueSize > 0) {
            const AVPacket &frontPkt = packets_.front();
            logger->info(
                "PacketQueue[{}]: size={}, totalSize={}, finished={}, front_packet: stream_index={}, pts={}, dts={}, size={}, flags={} (keyframe={})",
                queueName,
                queueSize,
                totalsize_,
                finished_,
                frontPkt.stream_index,
                frontPkt.pts,
                frontPkt.dts,
                frontPkt.size,
                frontPkt.flags,
                (frontPkt.flags & AV_PKT_FLAG_KEY) != 0 ? "yes" : "no");
        } else {
            logger->info("PacketQueue[{}]: size=0, totalSize={}, finished={}, empty", queueName, totalsize_, finished_);
        }
    }

    bool waitForSpace(size_t requiredSpace, int timeoutMs = 5000)
    {
        std::unique_lock<std::mutex> lck(mtx_);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        size_t requiredSize = totalsize_ + requiredSpace;

        while (totalsize_ + requiredSpace >= SizeLimit && !finished_ && !resetting_) {
            if (std::chrono::steady_clock::now() >= deadline) {
                logger->warn("PacketQueue[{}]: waitForSpace timeout after {} ms", "Unknown", timeoutMs);
                return false;
            }
            cond_.wait_for(lck, std::chrono::milliseconds(100));
        }
        return !resetting_; // 如果正在重置，返回 false
    }

    bool putBlocking(const AVPacket *pkt, const char *queueName = nullptr, int timeoutMs = 5000)
    {
        if (!pkt) {
            logger->warn("PacketQueue[{}]: putBlocking called with null packet", queueName ? queueName : "Unknown");
            return false;
        }

        size_t pktSize = static_cast<size_t>(pkt->size);

        try {
            if (!waitForSpace(pktSize, timeoutMs)) {
                logger->warn("PacketQueue[{}]: putBlocking timeout waiting for space", queueName ? queueName : "Unknown");
                return false;
            }

            {
                std::lock_guard<std::mutex> lck(mtx_);
                size_t oldSize = packets_.size();
                packets_.push_back(AVPacket{});
                if (av_packet_ref(&packets_.back(), pkt) != 0) {
                    logger->warn("PacketQueue[{}]: av_packet_ref failed", queueName ? queueName : "Unknown");
                    packets_.pop_back();
                    return false; // 修复：应该返回 false，表示失败
                }
                totalsize_ += static_cast<size_t>(packets_.back().size);
            }
            cond_.notify_one();
            return true;
        } catch (const std::exception &e) {
            logger->error("PacketQueue[{}]: Exception in putBlocking: {}", queueName ? queueName : "Unknown", e.what());
            return false;
        } catch (...) {
            logger->error("PacketQueue[{}]: Unknown exception in putBlocking", queueName ? queueName : "Unknown");
            return false;
        }
    }

    double getUsageRatio() const
    {
        std::lock_guard<std::mutex> lck(mtx_);
        return totalsize_ >= SizeLimit ? 1.0 : static_cast<double>(totalsize_) / SizeLimit;
    }

    bool IsEmpty() const
    {
        std::lock_guard<std::mutex> lck(const_cast<std::mutex &>(mtx_));
        return packets_.empty();
    }

    bool IsFinished() const
    {
        std::lock_guard<std::mutex> lck(const_cast<std::mutex &>(mtx_));
        return finished_;
    }

    size_t Size() const
    {
        std::lock_guard<std::mutex> lck(const_cast<std::mutex &>(mtx_));
        return packets_.size();
    }

    ///**
    // * 打印队列状态和首元素信息（用于调试）
    // * @param queueName 队列名称（如 "VideoQueue" 或 "AudioQueue"）
    // */
    //void logQueueState(const char *queueName)
    //{
    //    std::lock_guard<std::mutex> lck(mtx_);
    //    size_t queueSize = packets_.size();
    //    if (queueSize > 0) {
    //        const AVPacket &frontPkt = packets_.front();
    //        logger->info(
    //            "PacketQueue[{}]: size={}, totalSize={}, finished={}, front_packet: stream_index={}, pts={}, dts={}, size={}, flags={} (keyframe={})",
    //            queueName,
    //            queueSize,
    //            totalsize_,
    //            finished_,
    //            frontPkt.stream_index,
    //            frontPkt.pts,
    //            frontPkt.dts,
    //            frontPkt.size,
    //            frontPkt.flags,
    //            (frontPkt.flags & AV_PKT_FLAG_KEY) != 0 ? "yes" : "no");
    //    } else {
    //        logger->info("PacketQueue[{}]: size=0, totalSize={}, finished={}, empty", queueName, totalsize_, finished_);
    //    }
    //}

    /**
     * 获取下一个数据包（不发送到解码器，用于Decoder接口）
     * @return 数据包指针，如果没有数据包则返回nullptr
     * 注意：返回的指针在 Reset() 或 popPacket() 后可能失效，调用者应该立即使用
     */
    const AVPacket *peekPacket(const char *queueName = nullptr)
    {
        try {
            std::lock_guard<std::mutex> lck(mtx_);
            if (packets_.empty()) {
                return nullptr;
            }
            const AVPacket *pkt = &packets_.front();
            // 注意：返回的指针在锁释放后仍然有效，因为 packets_ 是 deque，不会移动元素
            // 但如果在 Reset() 或 popPacket() 后使用，指针会失效
            return pkt;
        } catch (const std::exception &e) {
            logger->error("PacketQueue[{}]: Exception in peekPacket: {}", queueName ? queueName : "Unknown", e.what());
            return nullptr;
        } catch (...) {
            logger->error("PacketQueue[{}]: Unknown exception in peekPacket", queueName ? queueName : "Unknown");
            return nullptr;
        }
    }

    /**
     * 移除并返回下一个数据包（用于Decoder接口）
     * @return 是否成功移除数据包
     */
    bool popPacket(const char *queueName = nullptr)
    {
        try {
            std::lock_guard<std::mutex> lck(mtx_);
            if (packets_.empty()) {
                return false;
            }
            AVPacket *pkt = &packets_.front();
            int64_t pts = pkt->pts;
            int stream_index = pkt->stream_index;
            int flags = pkt->flags;
            size_t oldSize = packets_.size();
            size_t pktSize = static_cast<size_t>(pkt->size);
            // 防止 totalsize_ 下溢
            if (pktSize > totalsize_) {
                logger->warn(
                    "PacketQueue[{}]: popPacket, pktSize ({}) > totalsize_ ({}), resetting totalsize_", queueName ? queueName : "Unknown", pktSize, totalsize_);
                totalsize_ = 0;
            } else {
                totalsize_ -= pktSize;
            }
            av_packet_unref(pkt);
            packets_.pop_front();
            cond_.notify_one();
            return true;
        } catch (const std::exception &e) {
            logger->error("PacketQueue[{}]: Exception in popPacket: {}", queueName ? queueName : "Unknown", e.what());
            return false;
        } catch (...) {
            logger->error("PacketQueue[{}]: Unknown exception in popPacket", queueName ? queueName : "Unknown");
            return false;
        }
    }

    // Seeking时清除旧数据使用
    bool Reset(const char *queueName = nullptr)
    {
        try {
            {
                std::lock_guard<std::mutex> guard(mtx_);
                // 设置重置标志，防止重置期间访问
                resetting_ = true;

                size_t oldSize = packets_.size();
                size_t oldTotalSize = totalsize_;
                for (AVPacket &pkt : packets_) {
                    try {
                        av_packet_unref(&pkt);
                    } catch (...) {
                        // 忽略单个数据包释放时的异常，继续处理
                        logger->warn("PacketQueue[{}]: Exception while unref packet in Reset", queueName ? queueName : "Unknown");
                    }
                }
                packets_.clear();
                totalsize_ = 0;
                finished_ = false;

                if (queueName && logger && oldSize > 0) {
                    logger->debug("PacketQueue[{}]: Reset, cleared {} packets", queueName, oldSize);
                }
            }

            // 在锁外通知等待的线程
            // 使用 std::weak_ptr 逻辑：即使 resetting_ 为 true，也要通知线程退出等待
            cond_.notify_all();

            // 等待一小段时间，确保所有线程都看到了 resetting_ 标志
            std::this_thread::sleep_for(std::chrono::milliseconds(10));

            // 清除重置标志
            {
                std::lock_guard<std::mutex> guard(mtx_);
                resetting_ = false;
            }

            return true;
        } catch (const std::exception &e) {
            logger->error("PacketQueue[{}]: Exception in Reset: {}", queueName ? queueName : "Unknown", e.what());
            // 确保重置标志被清除
            try {
                std::lock_guard<std::mutex> guard(mtx_);
                resetting_ = false;
            } catch (...) {
            }
            return false;
        } catch (...) {
            logger->error("PacketQueue[{}]: Unknown exception in Reset", queueName ? queueName : "Unknown");
            // 确保重置标志被清除
            try {
                std::lock_guard<std::mutex> guard(mtx_);
                resetting_ = false;
            } catch (...) {
            }
            return false;
        }
    }

private:
    AVPacket *getPacket(std::unique_lock<std::mutex> &lck)
    {
        // 检查是否正在重置，如果是则返回 nullptr
        if (resetting_) {
            return nullptr;
        }

        if (!cond_.wait_for(lck, std::chrono::milliseconds(50), [this]() { return !packets_.empty() || finished_ || resetting_; })) {
            return nullptr;
        }

        if (resetting_) {
            return nullptr;
        }

        return packets_.empty() ? nullptr : &packets_.front();
    }

    void pop()
    {
        try {
            if (packets_.empty()) {
                logger->warn("PacketQueue::pop called on empty queue");
                return;
            }
            AVPacket *pkt = &packets_.front();
            size_t pktSize = static_cast<size_t>(pkt->size);
            // 防止 totalsize_ 下溢
            if (pktSize > totalsize_) {
                logger->warn("PacketQueue::pop, pktSize ({}) > totalsize_ ({}), resetting totalsize_", pktSize, totalsize_);
                totalsize_ = 0;
            } else {
                totalsize_ -= pktSize;
            }
            av_packet_unref(pkt);
            packets_.pop_front();
        } catch (const std::exception &e) {
            logger->error("PacketQueue::pop Exception: {}", e.what());
        } catch (...) {
            logger->error("PacketQueue::pop Unknown exception");
        }
    }

    mutable std::mutex mtx_;
    std::condition_variable cond_;
    std::deque<AVPacket> packets_;
    size_t totalsize_{0};
    bool finished_{false};
    bool resetting_{false}; // 重置标志，防止重置期间访问队列

    // 队列满等待机制（防止视频队列满导致音频缓冲区一直满）
    bool waitForConsumer_{false};

    // 队列满统计（用于动态调整）
    size_t fullCount_{0};
    size_t lastFullSize_{0};
    std::chrono::steady_clock::time_point lastFullTime_;
};
