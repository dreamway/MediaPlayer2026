#pragma once

#include <atomic>
#include <thread>
#include <chrono>
#include <memory>
#include <QString>

class Movie;
class Video;
class Audio;

/**
 * 系统资源监控类
 * 定期监控CPU、内存、GPU使用情况以及编解码状态
 */
class SystemMonitor
{
public:
    SystemMonitor();
    ~SystemMonitor();

    // 启动监控线程（每5秒监控一次）
    void start(Movie* movie, Video* video, Audio* audio);
    
    // 停止监控线程
    void stop();

    // 获取当前进程的内存使用量（MB）
    double getMemoryUsageMB() const;
    
    // 获取当前进程的CPU使用率（百分比，0-100）
    // 注意：此函数会更新内部状态，不是真正的const
    double getCPUUsagePercent();
    
    // 获取GPU使用率（需要平台特定实现，暂时返回0）
    double getGPUUsagePercent() const;

private:
    // 监控线程主函数
    void monitorThread(Movie* movie, Video* video, Audio* audio);

    // 获取进程内存使用量（Windows平台）
    double getProcessMemoryUsage() const;
    
    // 获取进程CPU使用率（会更新内部状态）
    double getProcessCPUUsage();
    
    // 获取系统总内存（MB）
    double getTotalSystemMemoryMB() const;
    
    // 打印监控信息
    void printMonitorInfo(Movie* movie, Video* video, Audio* audio);

    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> monitorThread_;
    
    // CPU使用率计算相关
    std::chrono::steady_clock::time_point lastCPUCheckTime_;
    long long lastProcessTime_;  // 上次进程时间（tick count）
    
    // 监控间隔（毫秒）
    static constexpr int MONITOR_INTERVAL_MS = 5000;  // 5秒
};

