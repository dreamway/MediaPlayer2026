#include "PlayController.h"
#include "SystemMonitor.h"

#include "spdlog/spdlog.h"
#include "videoDecoder/OpenALAudio.h"
#include <cstddef>
#include <cstring>

#include <pdh.h>
#include <psapi.h>
#include <windows.h>
#pragma comment(lib, "pdh.lib")

SystemMonitor::SystemMonitor()
    : lastProcessTime_(0)
{}

SystemMonitor::~SystemMonitor()
{
    stop();
}

void SystemMonitor::start(PlayController *controller)
{
    if (running_.load()) {
        logger->warn("SystemMonitor already running");
        return;
    }

    running_.store(true);
    lastCPUCheckTime_ = std::chrono::steady_clock::now();

    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER time;
        time.LowPart = userTime.dwLowDateTime;
        time.HighPart = userTime.dwHighDateTime;
        lastProcessTime_ = time.QuadPart;
    }

    monitorThread_ = std::make_unique<std::thread>(&SystemMonitor::monitorThread, this, controller);
    logger->info("SystemMonitor started");
}

void SystemMonitor::stop()
{
    if (!running_.load()) {
        return;
    }

    running_.store(false);
    if (monitorThread_ && monitorThread_->joinable()) {
        monitorThread_->join();
    }
    logger->info("SystemMonitor stopped");
}

void SystemMonitor::monitorThread(PlayController *controller)
{
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_INTERVAL_MS));

        if (!running_.load()) {
            break;
        }

        printMonitorInfo(controller);
    }
}

double SystemMonitor::getMemoryUsageMB() const
{
    return getProcessMemoryUsage();
}

double SystemMonitor::getCPUUsagePercent()
{
    return getProcessCPUUsage();
}

double SystemMonitor::getGPUUsagePercent() const
{
    // GPU监控需要平台特定的API，暂时返回0
    // 可以使用NVIDIA NVML、AMD ADL或Windows WMI等
    return 0.0;
}

double SystemMonitor::getProcessMemoryUsage() const
{
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024.0 * 1024.0); // 转换为MB
    }
    return 0.0;
}

double SystemMonitor::getProcessCPUUsage()
{
    FILETIME creationTime, exitTime, kernelTime, userTime;
    FILETIME sysIdleTime, sysKernelTime, sysUserTime;

    if (!GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        return 0.0;
    }

    if (!GetSystemTimes(&sysIdleTime, &sysKernelTime, &sysUserTime)) {
        return 0.0;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCPUCheckTime_).count();

    if (elapsed < 100) {
        return 0.0; // 时间太短，返回0
    }

    ULARGE_INTEGER currentProcessTime, lastProcessTimeUL, sysKernelTimeUL, sysUserTimeUL;

    currentProcessTime.LowPart = userTime.dwLowDateTime;
    currentProcessTime.HighPart = userTime.dwHighDateTime;

    lastProcessTimeUL.QuadPart = lastProcessTime_;

    sysKernelTimeUL.LowPart = sysKernelTime.dwLowDateTime;
    sysKernelTimeUL.HighPart = sysKernelTime.dwHighDateTime;

    sysUserTimeUL.LowPart = sysUserTime.dwLowDateTime;
    sysUserTimeUL.HighPart = sysUserTime.dwHighDateTime;

    ULONGLONG totalSysTime = sysKernelTimeUL.QuadPart + sysUserTimeUL.QuadPart;
    ULONGLONG processTimeDelta = currentProcessTime.QuadPart - lastProcessTimeUL.QuadPart;

    // 更新上次的时间和进程时间
    lastCPUCheckTime_ = now;
    lastProcessTime_ = currentProcessTime.QuadPart;

    if (totalSysTime > 0) {
        double cpuPercent = (100.0 * processTimeDelta) / totalSysTime;
        return cpuPercent > 100.0 ? 100.0 : cpuPercent;
    }

    return 0.0;
}

double SystemMonitor::getTotalSystemMemoryMB() const
{
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullTotalPhys / (1024.0 * 1024.0); // 转换为MB
    }
    return 0.0;
}

void SystemMonitor::printMonitorInfo(PlayController *controller)
{
    if (!controller) {
        return;
    }

    double memMB = getMemoryUsageMB();
    double cpuPercent = getProcessCPUUsage();
    double totalMemMB = getTotalSystemMemoryMB();
    double memPercent = totalMemMB > 0 ? (memMB / totalMemMB) * 100.0 : 0.0;

    // 获取队列状态（需要通过friend访问，或者添加公共接口）
    // 这里先打印基本信息
    logger->info("========== System Monitor ==========");
    logger->info("Memory Usage: {:.2f} MB ({:.2f}% of system)", memMB, memPercent);
    logger->info("CPU Usage: {:.2f}%", cpuPercent);
    logger->info("GPU Usage: {:.2f}% (not implemented)", getGPUUsagePercent());

    // 打印播放状态
    if (controller) {
        logger->info("Playback State: {}", static_cast<int>(controller->getPlaybackState()));
        logger->info("Is Playing: {}", controller->isPlaying());
        logger->info("Is Paused: {}", controller->isPaused());
        logger->info("Is Stopped: {}", controller->isStopped());
        logger->info("Duration: {} ms", controller->getDurationMs());

        // 线程健康检查
        bool threadHealthy = controller->isThreadHealthy();
        logger->info("Thread Health: {}", threadHealthy ? "OK" : "WARNING");

        // 主动检测播放完成（如果线程不健康，可能是播放完成导致的）
        // 如果isPlaying()，但线程已经不健康，可能是播放完成但没有正确停止
        if (!threadHealthy && controller->isPlaying() && !controller->isPaused()) {
            // 尝试从MasterClock获取当前播放时间（估算）
            try {
                auto masterClock = controller->getMasterClock();
                int64_t currentSeconds = std::chrono::duration_cast<std::chrono::seconds>(masterClock).count();

                // 如果currentSeconds是负数或异常值，使用Duration作为估算值（假设播放到最后）
                if (currentSeconds < 0 || currentSeconds > controller->getDurationMs() / 1000 + 10) {
                    currentSeconds = controller->getDurationMs() / 1000; // 使用总时长作为估算
                }

                // 检查并触发停止（如果检测到播放完成）
                if (controller->checkAndStopIfFinished(currentSeconds)) {
                    logger->info("SystemMonitor: Detected and triggered playback finish stop");
                }
            } catch (...) {
                // 如果获取时钟失败，使用总时长作为估算值
                int64_t totalSeconds = controller->getDurationMs() / 1000;
                if (controller->checkAndStopIfFinished(totalSeconds)) {
                    logger->info("SystemMonitor: Detected and triggered playback finish stop (using duration as estimate)");
                }
            }
        }
    }

    // 注意：在新架构中，队列信息需要通过PlayController获取
    // 这里暂时只打印基本信息，避免访问已删除的Video类

    logger->info("===================================");
}
