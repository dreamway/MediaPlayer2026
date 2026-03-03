#include "PlayController.h"
#include "SystemMonitor.h"

#include "spdlog/spdlog.h"
#include "videoDecoder/OpenALAudio.h"
#include <cstddef>
#include <cstring>

#ifdef Q_OS_WIN
#include <pdh.h>
#include <psapi.h>
#include <windows.h>
#pragma comment(lib, "pdh.lib")
#else
#include <fstream>
#include <unistd.h>
#endif

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

#ifdef Q_OS_WIN
    FILETIME creationTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &creationTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER time;
        time.LowPart = userTime.dwLowDateTime;
        time.HighPart = userTime.dwHighDateTime;
        lastProcessTime_ = time.QuadPart;
    }
#else
    // Linux: read initial CPU time from /proc/self/stat
    std::ifstream statFile("/proc/self/stat");
    if (statFile.is_open()) {
        std::string ignore;
        long utime = 0, stime = 0;
        for (int i = 0; i < 13; ++i) statFile >> ignore;
        statFile >> utime >> stime;
        lastProcessTime_ = utime + stime;
    }
#endif

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
    return 0.0;
}

double SystemMonitor::getProcessMemoryUsage() const
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *) &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024.0 * 1024.0);
    }
    return 0.0;
#else
    std::ifstream statusFile("/proc/self/status");
    if (statusFile.is_open()) {
        std::string line;
        while (std::getline(statusFile, line)) {
            if (line.find("VmRSS:") == 0) {
                long rssKB = 0;
                sscanf(line.c_str(), "VmRSS: %ld", &rssKB);
                return rssKB / 1024.0;
            }
        }
    }
    return 0.0;
#endif
}

double SystemMonitor::getProcessCPUUsage()
{
#ifdef Q_OS_WIN
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
        return 0.0;
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

    lastCPUCheckTime_ = now;
    lastProcessTime_ = currentProcessTime.QuadPart;

    if (totalSysTime > 0) {
        double cpuPercent = (100.0 * processTimeDelta) / totalSysTime;
        return cpuPercent > 100.0 ? 100.0 : cpuPercent;
    }

    return 0.0;
#else
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastCPUCheckTime_).count();
    if (elapsed < 100) return 0.0;

    std::ifstream statFile("/proc/self/stat");
    if (!statFile.is_open()) return 0.0;

    std::string ignore;
    long utime = 0, stime = 0;
    for (int i = 0; i < 13; ++i) statFile >> ignore;
    statFile >> utime >> stime;
    long currentTime = utime + stime;

    long ticksPerSec = sysconf(_SC_CLK_TCK);
    double processTimeDelta = static_cast<double>(currentTime - lastProcessTime_) / ticksPerSec;
    double elapsedSec = elapsed / 1000.0;

    lastCPUCheckTime_ = now;
    lastProcessTime_ = currentTime;

    if (elapsedSec > 0) {
        double cpuPercent = (processTimeDelta / elapsedSec) * 100.0;
        return cpuPercent > 100.0 ? 100.0 : cpuPercent;
    }
    return 0.0;
#endif
}

double SystemMonitor::getTotalSystemMemoryMB() const
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&memInfo)) {
        return memInfo.ullTotalPhys / (1024.0 * 1024.0);
    }
    return 0.0;
#else
    std::ifstream meminfo("/proc/meminfo");
    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            if (line.find("MemTotal:") == 0) {
                long totalKB = 0;
                sscanf(line.c_str(), "MemTotal: %ld", &totalKB);
                return totalKB / 1024.0;
            }
        }
    }
    return 0.0;
#endif
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

    logger->info("========== System Monitor ==========");
    logger->info("Memory Usage: {:.2f} MB ({:.2f}% of system)", memMB, memPercent);
    logger->info("CPU Usage: {:.2f}%", cpuPercent);
    logger->info("GPU Usage: {:.2f}% (not implemented)", getGPUUsagePercent());

    if (controller) {
        logger->info("Playback State: {}", static_cast<int>(controller->getPlaybackState()));
        logger->info("Is Playing: {}", controller->isPlaying());
        logger->info("Is Paused: {}", controller->isPaused());
        logger->info("Is Stopped: {}", controller->isStopped());
        logger->info("Duration: {} ms", controller->getDurationMs());

        bool threadHealthy = controller->isThreadHealthy();
        logger->info("Thread Health: {}", threadHealthy ? "OK" : "WARNING");

        if (!threadHealthy && controller->isPlaying() && !controller->isPaused()) {
            try {
                auto masterClock = controller->getMasterClock();
                int64_t currentSeconds = std::chrono::duration_cast<std::chrono::seconds>(masterClock).count();

                if (currentSeconds < 0 || currentSeconds > controller->getDurationMs() / 1000 + 10) {
                    currentSeconds = controller->getDurationMs() / 1000;
                }

                if (controller->checkAndStopIfFinished(currentSeconds)) {
                    logger->info("SystemMonitor: Detected and triggered playback finish stop");
                }
            } catch (...) {
                int64_t totalSeconds = controller->getDurationMs() / 1000;
                if (controller->checkAndStopIfFinished(totalSeconds)) {
                    logger->info("SystemMonitor: Detected and triggered playback finish stop (using duration as estimate)");
                }
            }
        }
    }

    logger->info("===================================");
}
