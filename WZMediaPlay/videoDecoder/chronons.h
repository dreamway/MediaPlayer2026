#pragma once

#include <chrono>

inline constexpr int64_t operator"" _i64(unsigned long long int n) noexcept {
    return static_cast<int64_t>(n);
}
using fixed32 = std::chrono::duration<int64_t, std::ratio<1, (1_i64 << 32)>>;
using nanoseconds = std::chrono::nanoseconds;
using microseconds = std::chrono::microseconds;
using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
using seconds_d64 = std::chrono::duration<double>;

/**
 * 时间戳工具函数：替代 nanoseconds::min() 作为"无效值"哨兵
 * 
 * 约定：nanoseconds::min() 表示"无效/未设置"，但对其做算术运算会溢出。
 * 使用这些辅助函数使语义显式化，避免意外的算术溢出。
 */
inline constexpr nanoseconds kInvalidTimestamp = nanoseconds::min();

inline bool isValidTimestamp(nanoseconds ts) {
    return ts != kInvalidTimestamp;
}

inline nanoseconds safeTimestampAdd(nanoseconds ts, nanoseconds delta) {
    if (!isValidTimestamp(ts)) return kInvalidTimestamp;
    return ts + delta;
}
