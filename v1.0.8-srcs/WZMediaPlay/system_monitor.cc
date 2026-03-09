#include "system_monitor.h"
#include "movie.h"
#include "videoDecoder/video.h"
#include "videoDecoder/audio.h"
#include "spdlog/spdlog.h"
#include <cstring>
#include <cstddef>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#pragma comment(lib, "pdh.lib")
#else
#include <unistd.h>
#include <sys/resource.h>
#include <sys/times.h>
#endif

SystemMonitor::SystemMonitor()
    : lastProcessTime_(0)
{
}

SystemMonitor::~SystemMonitor()
{
    stop();
}

void SystemMonitor::start(Movie* movie, Video* video, Audio* audio)
{
    if (running_.load()) {
        logger->warn("SystemMonitor already running");
        return;
    }
    
    running_.store(true);
    lastCPUCheckTime_ = std::chrono::steady_clock::now();
    
#ifdef _WIN32
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER time;
        time.LowPart = userTime.dwLowDateTime;
        time.HighPart = userTime.dwHighDateTime;
        lastProcessTime_ = time.QuadPart;
    }
#endif
    
    monitorThread_ = std::make_unique<std::thread>(&SystemMonitor::monitorThread, this, movie, video, audio);
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

void SystemMonitor::monitorThread(Movie* movie, Video* video, Audio* audio)
{
    while (running_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(MONITOR_INTERVAL_MS));
        
        if (!running_.load()) {
            break;
        }
        
        printMonitorInfo(movie, video, audio);
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
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024.0 * 1024.0);  // 转换为MB
    }
    return 0.0;
#else
    // Linux/Mac实现
    FILE* file = fopen("/proc/self/status", "r");
    if (file) {
        char line[128];
        while (fgets(line, 128, file)) {
            if (strncmp(line, "VmRSS:", 6) == 0) {
                double mem = 0;
                sscanf(line, "VmRSS: %lf kB", &mem);
                fclose(file);
                return mem / 1024.0;  // 转换为MB
            }
        }
        fclose(file);
    }
    return 0.0;
#endif
}

double SystemMonitor::getProcessCPUUsage()
{
#ifdef _WIN32
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
        return 0.0;  // 时间太短，返回0
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
#else
    // Linux实现
    static clock_t lastCPU = 0;
    static clock_t lastSysCPU = 0;
    static clock_t lastUserCPU = 0;
    static int numProcessors = sysconf(_SC_NPROCESSORS_ONLN);
    
    struct tms timeSample;
    clock_t now = times(&timeSample);
    
    if (lastCPU == 0 || lastSysCPU == 0 || lastUserCPU == 0) {
        lastCPU = now;
        lastSysCPU = timeSample.tms_stime;
        lastUserCPU = timeSample.tms_utime;
        return 0.0;
    }
    
    double percent = 0.0;
    clock_t total = (timeSample.tms_stime - lastSysCPU) + (timeSample.tms_utime - lastUserCPU);
    clock_t totalSinceLastCheck = now - lastCPU;
    
    if (totalSinceLastCheck > 0 && numProcessors > 0) {
        percent = (100.0 * total) / (totalSinceLastCheck * numProcessors);
    }
    
    lastCPU = now;
    lastSysCPU = timeSample.tms_stime;
    lastUserCPU = timeSample.tms_utime;
    
    return percent > 100.0 ? 100.0 : percent;
#endif
}

double SystemMonitor::getTotalSystemMemoryMB() const
{
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullTotalPhys / (1024.0 * 1024.0);  // 转换为MB
    }
    return 0.0;
#else
    // Linux实现
    FILE* file = fopen("/proc/meminfo", "r");
    if (file) {
        char line[128];
        while (fgets(line, 128, file)) {
            if (strncmp(line, "MemTotal:", 9) == 0) {
                double mem = 0;
                sscanf(line, "MemTotal: %lf kB", &mem);
                fclose(file);
                return mem / 1024.0;  // 转换为MB
            }
        }
        fclose(file);
    }
    return 0.0;
#endif
}

void SystemMonitor::printMonitorInfo(Movie* movie, Video* video, Audio* audio)
{
    if (!movie || !video) {
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
    if (movie) {
        logger->info("Play State: {}", static_cast<int>(movie->GetPlayState()));
        logger->info("Is Playing: {}", movie->IsPlaying());
        logger->info("Is Paused: {}", movie->IsPaused());
        logger->info("Is Stopped: {}", movie->IsStopped());
        logger->info("Duration: {} ms", movie->GetDurationInMs());
        
        // 线程健康检查
        bool threadHealthy = movie->isThreadHealthy();
        logger->info("Thread Health: {}", threadHealthy ? "OK" : "WARNING");
        
        // 主动检测播放完成（如果线程不健康，可能是播放完成导致的）
        // 如果playState还是PLAYING，但线程已经不健康，可能是播放完成但没有正确停止
        if (!threadHealthy && movie->IsPlaying() && !movie->IsPaused()) {
            // 尝试从MasterClock获取当前播放时间（估算）
            try {
                auto masterClock = movie->getMasterClock();
                int64_t currentSeconds = std::chrono::duration_cast<std::chrono::seconds>(masterClock).count();
                
                // 如果currentSeconds是负数或异常值，使用Duration作为估算值（假设播放到最后）
                if (currentSeconds < 0 || currentSeconds > movie->GetDurationInMs() / 1000 + 10) {
                    currentSeconds = movie->GetDurationInMs() / 1000;  // 使用总时长作为估算
                }
                
                // 检查并触发停止（如果检测到播放完成）
                if (movie->checkAndStopIfFinished(currentSeconds)) {
                    logger->info("SystemMonitor: Detected and triggered playback finish stop");
                }
            } catch (...) {
                // 如果获取时钟失败，使用总时长作为估算值
                int64_t totalSeconds = movie->GetDurationInMs() / 1000;
                if (movie->checkAndStopIfFinished(totalSeconds)) {
                    logger->info("SystemMonitor: Detected and triggered playback finish stop (using duration as estimate)");
                }
            }
        }
    }
    
    // 打印视频队列状态和FPS
    if (video) {
        size_t readIdx = video->getQueueReadIndex();
        size_t writeIdx = video->getQueueWriteIndex();
        size_t queueSize = video->getQueueSize();
        size_t usedSlots = (writeIdx >= readIdx) ? (writeIdx - readIdx) : (queueSize - readIdx + writeIdx);
        logger->info("Picture Queue: read_idx={}, write_idx={}, size={}, used={}/{}", 
                    readIdx, writeIdx, queueSize, usedSlots, queueSize);
        
        // 计算FPS（基于队列写入速度）
        // 使用静态变量跟踪FPS
        static std::chrono::steady_clock::time_point lastFpsCheckTime = std::chrono::steady_clock::now();
        static size_t lastWriteIdx = SIZE_MAX;  // 初始化为无效值
        static float currentFPS = 0.0f;
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsCheckTime).count();
        
        // 每秒更新一次FPS（基于队列写入速度）
        if (elapsed >= 1000 && lastWriteIdx != SIZE_MAX) {
            // 计算写入的帧数（考虑环形缓冲区）
            size_t framesWritten = 0;
            if (writeIdx >= lastWriteIdx) {
                framesWritten = writeIdx - lastWriteIdx;
            } else {
                // 环形缓冲区回绕
                framesWritten = (queueSize - lastWriteIdx) + writeIdx;
            }
            
            if (elapsed > 0) {
                currentFPS = (framesWritten * 1000.0f) / elapsed;
            }
            lastWriteIdx = writeIdx;
            lastFpsCheckTime = now;
        } else if (lastWriteIdx == SIZE_MAX) {
            // 首次调用，初始化
            lastWriteIdx = writeIdx;
            lastFpsCheckTime = now;
        }
        
        logger->info("Video FPS: {:.1f} (decoded frames per second)", currentFPS);
    }
    
    logger->info("===================================");
}

