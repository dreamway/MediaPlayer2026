#include "FrameBuffer.h"
#include "spdlog/spdlog.h"

extern spdlog::logger *logger;

FrameBuffer::FrameBuffer(size_t capacity)
    : capacity_(capacity)
{
    frames_.resize(capacity_);
    // 初始化所有帧为空（新的Frame类会自动处理）
    for (auto& frame : frames_) {
        frame.clear();
    }
}

FrameBuffer::~FrameBuffer()
{
    clear();
}

bool FrameBuffer::putFrame(Frame&& frame, bool waitIfFull)
{
    std::unique_lock<std::mutex> lock(mutex_);
    
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    size_t currentReadIdx = readIdx_.load(std::memory_order_acquire);
    
    // 计算下一个写位置
    size_t nextWriteIdx = (currentWriteIdx + 1) % capacity_;
    
    // 检查队列是否满（写索引追上读索引）
    if (nextWriteIdx == currentReadIdx) {
        if (waitIfFull) {
            // 队列满，等待有空间（等待消费者消费帧）
            // 使用条件变量等待，直到队列有空间
            // 注意：使用带超时的 wait_for，避免无限等待导致线程卡住
            auto timeout = std::chrono::milliseconds(100);  // 100ms 超时
            bool spaceAvailable = cond_.wait_for(lock, timeout, [this, currentReadIdx]() {
                size_t newReadIdx = readIdx_.load(std::memory_order_acquire);
                size_t newWriteIdx = writeIdx_.load(std::memory_order_acquire);
                size_t nextWriteIdx = (newWriteIdx + 1) % capacity_;
                return nextWriteIdx != newReadIdx;  // 有空间时返回true
            });
            
            if (!spaceAvailable) {
                // 超时，返回 false（让调用者决定是否重试或丢弃帧）
                nanoseconds pts = frame.ts();
                logger->warn("FrameBuffer::putFrame: wait timeout ({}ms), buffer still full, pts={}ns", 
                            timeout.count(), pts.count());
                return false;
            }
            
            // 重新获取索引（可能已经变化）
            currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
            currentReadIdx = readIdx_.load(std::memory_order_acquire);
            nextWriteIdx = (currentWriteIdx + 1) % capacity_;
            
            // 再次检查（虽然理论上不应该再满，但为了安全）
            if (nextWriteIdx == currentReadIdx) {
                logger->error("FrameBuffer::putFrame: buffer still full after wait, this should not happen");
                return false;
            }
        } else {
            nanoseconds pts = frame.ts();
            logger->warn("FrameBuffer::putFrame: buffer full, dropping frame with pts={}ns", 
                         pts.count());
            return false;
        }
    }
    
    // 写入帧
    frames_[currentWriteIdx] = std::move(frame);
    writeIdx_.store(nextWriteIdx, std::memory_order_release);
    
    // 通知等待的消费者
    cond_.notify_one();
    
    return true;
}

bool FrameBuffer::getFrame(Frame& frame, const GetFrameOptions& options)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t currentReadIdx = readIdx_.load(std::memory_order_acquire);
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    
    // 检查队列是否为空
    if (currentReadIdx == currentWriteIdx) {
        return false;
    }
    
    // 如果正在 Seeking，返回 false（不读取帧）
    if (options.isSeeking) {
        return false;
    }
    
    // 确定有效时钟时间
    nanoseconds effectiveClock = options.audioClockStuck || options.audioFinished 
        ? options.videoClock 
        : options.clockTime;
    
    // 计算帧过期阈值（根据视频帧率动态调整）
    nanoseconds expiredThreshold = calculateExpiredThreshold(options.videoFrameRate);
    
    // 查找当前有效帧
    size_t currentIdx = currentReadIdx;
    Frame* currentFrame = &frames_[currentIdx];
    
    // 如果当前帧无效，尝试查找下一个有效帧
    if (currentFrame->isEmpty() || currentFrame->ts() == nanoseconds::min()) {
        size_t nextValidIdx = findNextValidFrame(currentIdx);
        if (nextValidIdx == capacity_) {
            // 没有找到有效帧
            return false;
        }
        currentIdx = nextValidIdx;
        currentFrame = &frames_[currentIdx];
    }
    
    // 检查当前帧是否过期
    nanoseconds currentFramePts = currentFrame->ts();
    bool currentFrameExpired = isFrameExpired(currentFramePts, effectiveClock, options.videoFrameRate);
    
    if (currentFrameExpired) {
        logger->debug("FrameBuffer::getFrame: current frame expired, pts={}ns, clock={}ns, lag={}ns",
                     currentFramePts.count(), effectiveClock.count(),
                     (effectiveClock - currentFramePts).count());
        
        // 帧过期，尝试跳过过期帧（限制跳过的帧数）
        size_t skipCount = 0;
        size_t nextIdx = (currentIdx + 1) % capacity_;
        
        while (skipCount < MAX_SKIP_FRAMES && nextIdx != currentWriteIdx) {
            Frame* nextFrame = &frames_[nextIdx];
            
            // 如果下一帧无效，停止
            if (nextFrame->isEmpty() || nextFrame->ts() == nanoseconds::min()) {
                break;
            }
            
            // 检查下一帧是否也过期
            nanoseconds nextFramePts = nextFrame->ts();
            nanoseconds nextFrameLag = effectiveClock - nextFramePts;
            if (nextFrameLag > expiredThreshold) {
                // 下一帧也过期，继续跳过
                skipCount++;
                nextIdx = (nextIdx + 1) % capacity_;
            } else {
                // 找到未过期的帧
                currentIdx = nextIdx;
                currentFrame = nextFrame;
                break;
            }
        }
        
        // 如果跳过了太多帧，或者所有帧都过期，使用当前帧（避免跳帧过多）
        // 但需要验证当前帧是否有效，如果无效则不返回
        if (skipCount >= MAX_SKIP_FRAMES || nextIdx == currentWriteIdx) {
            logger->warn("FrameBuffer::getFrame: too many expired frames, using current frame");
            // 验证当前帧是否有效（使用新的Frame类的isEmpty()方法）
            if (currentFrame && !currentFrame->isEmpty()) {
                // 帧有效，继续使用
            } else {
                // 当前帧也无效，返回false（不返回无效帧）
                logger->warn("FrameBuffer::getFrame: current frame is also invalid, returning false");
                return false;
            }
        }
    } else {
        // 当前帧未过期，检查是否有更接近时钟时间的帧
        if (options.videoFrameRate > 0) {
            // 查找最接近时钟时间的帧（在合理范围内）
            size_t closestIdx = findClosestFrame(effectiveClock, currentIdx);
            if (closestIdx != capacity_) {
                nanoseconds currentDiff = effectiveClock - currentFramePts;
                nanoseconds closestDiff = effectiveClock - frames_[closestIdx].ts();
                int64_t currentLagAbs = std::abs(currentDiff.count());
                int64_t closestLagAbs = std::abs(closestDiff.count());
                
                // 如果找到的帧更接近，且差异明显（超过一帧的时间），使用它
                int64_t frameTimeNs = static_cast<int64_t>(1000000000.0 / options.videoFrameRate);
                int64_t lagDiff = currentLagAbs - closestLagAbs;
                if (closestLagAbs < currentLagAbs && lagDiff > (frameTimeNs / 2)) {
                    currentIdx = closestIdx;
                    currentFrame = &frames_[currentIdx];
                }
            }
        }
    }
    
    // 验证帧的有效性（防止返回无效帧导致黑画面）
    // 使用新的Frame类的isEmpty()方法统一检查
    if (currentFrame->isEmpty()) {
        logger->warn("FrameBuffer::getFrame: Selected frame is invalid (width={}, height={}, format={}), skipping", 
                    currentFrame->width(), currentFrame->height(), 
                    static_cast<int>(currentFrame->pixelFormat()));
        // 跳过无效帧，更新读索引，继续查找下一个有效帧
        size_t nextReadIdx = (currentIdx + 1) % capacity_;
        readIdx_.store(nextReadIdx, std::memory_order_release);
        cond_.notify_one();  // 通知生产者
        return false;  // 不返回无效帧
    }
    
    // 返回选中的帧
    frame = std::move(*currentFrame);
    
    // 更新读索引
    size_t nextReadIdx = (currentIdx + 1) % capacity_;
    readIdx_.store(nextReadIdx, std::memory_order_release);
    
    // 通知等待的生产者（putFrame可能在等待空间）
    cond_.notify_one();
    
    return true;
}

bool FrameBuffer::getFrame(Frame& frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    size_t currentReadIdx = readIdx_.load(std::memory_order_acquire);
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    
    // 检查队列是否为空
    if (currentReadIdx == currentWriteIdx) {
        return false;
    }
    
    // 简单返回当前帧（不进行同步处理）
    frame = std::move(frames_[currentReadIdx]);
    
    // 更新读索引
    size_t nextReadIdx = (currentReadIdx + 1) % capacity_;
    readIdx_.store(nextReadIdx, std::memory_order_release);
    
    // 通知等待的生产者（putFrame可能在等待空间）
    cond_.notify_one();
    
    return true;
}

void FrameBuffer::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 清空所有帧（使用新的Frame类的clear()方法）
    for (auto& frame : frames_) {
        frame.clear();
    }
    
    // 重置读写索引（确保 readIdx != writeIdx）
    readIdx_.store(0, std::memory_order_release);
    writeIdx_.store(1, std::memory_order_release);
    
    // 通知所有等待的线程
    cond_.notify_all();
    
    if (logger) {
        logger->debug("FrameBuffer::clear: buffer cleared, readIdx=0, writeIdx=1");
    }
}

size_t FrameBuffer::size() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return calculateSize();
}

bool FrameBuffer::isEmpty() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return readIdx_.load(std::memory_order_acquire) == writeIdx_.load(std::memory_order_acquire);
}

bool FrameBuffer::isFull() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    size_t currentReadIdx = readIdx_.load(std::memory_order_acquire);
    size_t nextWriteIdx = (currentWriteIdx + 1) % capacity_;
    return nextWriteIdx == currentReadIdx;
}

size_t FrameBuffer::calculateSize() const
{
    size_t currentReadIdx = readIdx_.load(std::memory_order_acquire);
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    
    if (currentWriteIdx >= currentReadIdx) {
        return currentWriteIdx - currentReadIdx;
    } else {
        return capacity_ - currentReadIdx + currentWriteIdx;
    }
}

bool FrameBuffer::isFrameExpired(nanoseconds framePts, nanoseconds clockTime, double frameRate) const
{
    if (framePts == nanoseconds::min()) {
        return false;  // 无效帧不算过期
    }
    
    nanoseconds lag = clockTime - framePts;
    nanoseconds threshold = calculateExpiredThreshold(frameRate);
    
    return lag > threshold;
}

nanoseconds FrameBuffer::calculateExpiredThreshold(double frameRate) const
{
    if (frameRate > 0) {
        // 根据视频帧率动态调整：2-3帧的时间
        nanoseconds frameTime = nanoseconds(static_cast<int64_t>(1000000000.0 / frameRate));
        return frameTime * 3;  // 3帧的时间
    }
    
    // 默认阈值：1秒
    return DEFAULT_EXPIRED_THRESHOLD;
}

size_t FrameBuffer::findNextValidFrame(size_t startIdx) const
{
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    size_t idx = (startIdx + 1) % capacity_;
    
    while (idx != currentWriteIdx) {
        // 使用新的Frame类的isEmpty()方法检查有效性
        if (!frames_[idx].isEmpty() && frames_[idx].ts() != nanoseconds::min()) {
            return idx;
        }
        idx = (idx + 1) % capacity_;
    }
    
    return capacity_;  // 未找到
}

size_t FrameBuffer::findClosestFrame(nanoseconds clockTime, size_t startIdx) const
{
    size_t currentWriteIdx = writeIdx_.load(std::memory_order_acquire);
    size_t bestIdx = capacity_;
    int64_t bestDiff = std::numeric_limits<int64_t>::max();
    
    size_t idx = startIdx;
    int count = 0;
    const int MAX_SEARCH_COUNT = 5;  // 最多搜索5帧
    
    while (idx != currentWriteIdx && count < MAX_SEARCH_COUNT) {
        // 使用新的Frame类的ts()方法获取时间戳
        if (!frames_[idx].isEmpty()) {
            nanoseconds framePts = frames_[idx].ts();
            if (framePts != nanoseconds::min()) {
                nanoseconds diff = clockTime - framePts;
                int64_t diffAbs = std::abs(diff.count());
                if (diffAbs < bestDiff) {
                    bestDiff = diffAbs;
                    bestIdx = idx;
                }
            }
        }
        idx = (idx + 1) % capacity_;
        count++;
    }
    
    return bestIdx;
}
