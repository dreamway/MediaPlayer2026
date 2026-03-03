#pragma once

#include "chronons.h"
#include "ffmpeg.h"
#include "Frame.h"  // 使用新的统一 Frame 类

#include <array>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <vector>
#include <limits>

/**
 * FrameBuffer: 视频帧缓存管理器
 * 
 * 职责：
 * - 线程安全的帧存储和读取
 * - 根据时钟时间选择合适的帧（音视频同步）
 * - 处理帧过期和跳帧
 * - 确保 Seeking 后状态正确重置
 * 
 * 设计特点：
 * - 使用环形缓冲区（Ring Buffer）实现
 * - 线程安全：使用 mutex 保护关键操作
 * - 解决 read_idx == write_idx 的问题（初始状态 writeIdx = 1）
 * - 集成音视频同步逻辑
 * 
 * 注意：现在使用统一的 Frame 类，不再区分 frame/sw_frame/yuv420p_frame
 */
class FrameBuffer
{
public:
    // 使用统一的 Frame 类（不再需要内部 Frame 结构体）
    using Frame = ::Frame;  // 使用全局 Frame 类

    /**
     * GetFrameOptions: getFrame 的选项
     * 用于控制帧选择行为
     */
    struct GetFrameOptions {
        nanoseconds clockTime{0};           // 当前时钟时间（用于音视频同步）
        nanoseconds videoClock{0};          // 视频时钟（fallback）
        bool audioFinished{false};          // 音频是否已结束
        bool audioClockStuck{false};        // 音频时钟是否卡住
        bool isSeeking{false};              // 是否正在 Seeking
        double videoFrameRate{0.0};         // 视频帧率（用于动态调整阈值）
        
        GetFrameOptions() = default;
    };

    /**
     * 构造函数
     * @param capacity 缓冲区容量（默认120帧，用于预加载更多帧，避免高码率高FPS视频的黑画面）
     */
    explicit FrameBuffer(size_t capacity = 120);
    
    ~FrameBuffer();

    /**
     * putFrame: 写入帧（生产者调用）
     * @param frame 要写入的帧
     * @param waitIfFull 如果队列满，是否等待（默认true，等待队列有空间）
     * @return 成功返回 true，队列满且不等待返回 false
     */
    bool putFrame(Frame&& frame, bool waitIfFull = true);

    /**
     * getFrame: 读取帧（消费者调用）
     * 根据时钟时间选择合适的帧，处理帧过期和跳帧
     * 
     * @param frame 输出参数，返回选中的帧
     * @param options 帧选择选项（时钟时间、同步模式等）
     * @return 成功返回 true，无可用帧返回 false
     */
    bool getFrame(Frame& frame, const GetFrameOptions& options);

    /**
     * getFrame: 简化版本（仅用于测试或简单场景）
     * @param frame 输出参数
     * @return 成功返回 true
     */
    bool getFrame(Frame& frame);

    /**
     * clear: 清空缓冲区（线程安全）
     * 确保 Seeking 后状态正确重置
     */
    void clear();

    /**
     * 状态查询
     */
    size_t size() const;
    bool isEmpty() const;
    bool isFull() const;
    size_t getCapacity() const { return capacity_; }

    /**
     * 获取读写索引（用于监控和调试）
     */
    size_t getReadIndex() const { return readIdx_.load(std::memory_order_acquire); }
    size_t getWriteIndex() const { return writeIdx_.load(std::memory_order_acquire); }

private:
    /**
     * 计算队列中有效帧的数量
     */
    size_t calculateSize() const;

    /**
     * 检查帧是否过期
     * @param framePts 帧的 PTS
     * @param clockTime 当前时钟时间
     * @param frameRate 视频帧率
     * @return 过期返回 true
     */
    bool isFrameExpired(nanoseconds framePts, nanoseconds clockTime, double frameRate) const;

    /**
     * 计算帧过期阈值（根据视频帧率动态调整）
     * @param frameRate 视频帧率
     * @return 过期阈值（纳秒）
     */
    nanoseconds calculateExpiredThreshold(double frameRate) const;

    /**
     * 查找下一个有效帧的索引
     * @param startIdx 起始索引
     * @return 找到返回索引，未找到返回 capacity_（无效）
     */
    size_t findNextValidFrame(size_t startIdx) const;

    /**
     * 查找最接近时钟时间的帧索引
     * @param clockTime 时钟时间
     * @param startIdx 起始索引
     * @return 找到返回索引，未找到返回 capacity_（无效）
     */
    size_t findClosestFrame(nanoseconds clockTime, size_t startIdx) const;

private:
    std::vector<Frame> frames_;              // 帧缓冲区
    std::atomic<size_t> readIdx_{0};         // 读索引
    std::atomic<size_t> writeIdx_{1};       // 写索引（初始为1，确保 readIdx != writeIdx）
    mutable std::mutex mutex_;               // 保护关键操作的互斥锁
    std::condition_variable cond_;           // 条件变量（用于等待）
    size_t capacity_;                        // 缓冲区容量
    
    // 帧过期检测相关
    static constexpr nanoseconds DEFAULT_EXPIRED_THRESHOLD{1LL * 1000000000};  // 默认1秒
    static constexpr size_t MAX_SKIP_FRAMES = 3;  // 每次最多跳过的帧数
};
