#pragma once

#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>

// Log one line to stdout (journald captures it when running under systemd).
#define PLOG(...)                                   \
    do {                                            \
        std::fprintf(stdout, "[picam] " __VA_ARGS__); \
        std::fputc('\n', stdout);                   \
        std::fflush(stdout);                        \
    } while (0)

namespace util {

using Clock = std::chrono::steady_clock;

inline double nowMs() {
    return std::chrono::duration<double, std::milli>(Clock::now().time_since_epoch()).count();
}

// Seconds since the kernel started (/proc/uptime). -1 if unavailable.
inline double uptimeSeconds() {
    std::ifstream f("/proc/uptime");
    double up = -1.0;
    if (f) f >> up;
    return up;
}

// SoC temperature in degrees C. -1 if unavailable.
inline double cpuTempC() {
    std::ifstream f("/sys/class/thermal/thermal_zone0/temp");
    long milli = 0;
    if (f >> milli) return static_cast<double>(milli) / 1000.0;
    return -1.0;
}

// MemAvailable in MB. -1 if unavailable.
inline long memAvailableMb() {
    std::ifstream f("/proc/meminfo");
    std::string line;
    while (std::getline(f, line)) {
        long kb = 0;
        if (std::sscanf(line.c_str(), "MemAvailable: %ld kB", &kb) == 1) return kb / 1024;
    }
    return -1;
}

}  // namespace util
