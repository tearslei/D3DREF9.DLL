#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <array>
#include <utility>
#include <chrono>
#include <random>

namespace d3dref9 {
using Clock = std::chrono::steady_clock;
inline void SleepMs(uint32_t ms) { ::Sleep(ms); }
}
