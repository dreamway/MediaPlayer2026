#pragma once

#include "ffmpeg.h"

#include <cassert>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#include "../GlobalDef.h"
#include "spdlog/spdlog.h"
#include <QDebug>
#include <QtLogging>

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

    int sendTo(AVCodecContext *codecctx)
    {
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

    bool put(const AVPacket *pkt)
    {
        {
            std::lock_guard<std::mutex> lck(mtx_);
            if (totalsize_ >= SizeLimit) {
                logger->debug("totalSize={}, >=SizeLimit:{}, return false", totalsize_, SizeLimit);
                return false;
            }
            packets_.push_back(AVPacket{});
            if (av_packet_ref(&packets_.back(), pkt) != 0) {
                assert(0);
                logger->warn("av_packet_ref !=0");
                packets_.pop_back();
                return true;
            }
            totalsize_ += static_cast<size_t>(packets_.back().size);
        }
        cond_.notify_one();
        return true;
    }

    bool IsEmpty() { return packets_.empty(); }

    size_t Size() { return packets_.size(); }

    // Seeking时清除旧数据使用
    bool Reset()
    {
        {
            std::lock_guard<std::mutex> guard(mtx_);

            for (AVPacket &pkt : packets_) {
                av_packet_unref(&pkt);
            }
            packets_.clear();
            totalsize_ = 0;
            logger->info("Packet_queue.Reset, totalSize_={}", totalsize_);
            finished_ = false;
        }
        cond_.notify_one();
        return true;
    }

private:
    AVPacket *getPacket(std::unique_lock<std::mutex> &lck)
    {
        // 添加超时等待，防止永久阻塞
        if (!cond_.wait_for(lck, std::chrono::milliseconds(50), 
            [this]() { return !packets_.empty() || finished_; })) {
            logger->debug("PacketQueue timeout waiting for packet");
            return nullptr;  // 超时返回nullptr
        }
        return packets_.empty() ? nullptr : &packets_.front();
    }

    void pop()
    {
        AVPacket *pkt = &packets_.front();
        totalsize_ -= static_cast<size_t>(pkt->size);
        av_packet_unref(pkt);
        packets_.pop_front();
    }

    std::mutex mtx_;
    std::condition_variable cond_;
    std::deque<AVPacket> packets_;
    size_t totalsize_{0};
    bool finished_{false};
};
