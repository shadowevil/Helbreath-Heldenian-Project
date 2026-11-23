#pragma once

// benchmark.h - Lightweight performance profiling macros and debug console
// Outputs to debug console window when allocated

#include <cstdio>
#include <cstring>
#include <chrono>

#include "platform_headers.h"

#ifdef _WIN32
#include <psapi.h>
#endif

// ============================================================================
// Debug Console
// ============================================================================

class DebugConsole {
private:
    static bool s_bAllocated;
    static FILE* s_pOut;
    static FILE* s_pErr;
#ifdef _WIN32
    static SIZE_T s_lastWorkingSet;
#endif

public:
    static void allocate()
    {
#ifdef _WIN32
        if (s_bAllocated) return;

        if (AllocConsole())
        {
            freopen_s(&s_pOut, "CONOUT$", "w", stdout);
            freopen_s(&s_pErr, "CONOUT$", "w", stderr);

            SetConsoleTitleA("Helbreath Debug Console");

            // enable ANSI colors (Windows 10+)
            HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD mode = 0;
            GetConsoleMode(hOut, &mode);
            SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

            s_bAllocated = true;
            s_lastWorkingSet = 0;
            printf("=== Helbreath Debug Console ===\n\n");
        }
#endif
    }

    static void deallocate()
    {
#ifdef _WIN32
        if (!s_bAllocated) return;

        if (s_pOut) fclose(s_pOut);
        if (s_pErr) fclose(s_pErr);
        s_pOut = nullptr;
        s_pErr = nullptr;

        FreeConsole();
        s_bAllocated = false;
#endif
    }

    static bool is_allocated() { return s_bAllocated; }

    static void print_memory(const char* label)
    {
#ifdef _WIN32
        if (!s_bAllocated) return;

        PROCESS_MEMORY_COUNTERS_EX pmc;
        if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
        {
            double working_mb = pmc.WorkingSetSize / (1024.0 * 1024.0);
            double private_mb = pmc.PrivateUsage / (1024.0 * 1024.0);
            double peak_mb = pmc.PeakWorkingSetSize / (1024.0 * 1024.0);

            if (s_lastWorkingSet > 0)
            {
                double delta_mb = static_cast<double>((LONGLONG)pmc.WorkingSetSize - (LONGLONG)s_lastWorkingSet) / (1024.0 * 1024.0);
                const char* sign = (delta_mb >= 0) ? "+" : "";
                printf("[MEMORY] %-20s Working: %6.2f MB | Private: %6.2f MB | Peak: %6.2f MB (%s%.2f MB)\n",
                    label, working_mb, private_mb, peak_mb, sign, delta_mb);
            }
            else
            {
                printf("[MEMORY] %-20s Working: %6.2f MB | Private: %6.2f MB | Peak: %6.2f MB\n",
                    label, working_mb, private_mb, peak_mb);
            }

            s_lastWorkingSet = pmc.WorkingSetSize;
        }
#endif
    }

    static void reset_memory_delta()
    {
#ifdef _WIN32
        s_lastWorkingSet = 0;
#endif
    }
};

// Static member initialization
inline bool DebugConsole::s_bAllocated = false;
inline FILE* DebugConsole::s_pOut = nullptr;
inline FILE* DebugConsole::s_pErr = nullptr;
#ifdef _WIN32
inline SIZE_T DebugConsole::s_lastWorkingSet = 0;
#endif

// ============================================================================
// Benchmark Macros
// ============================================================================

#ifdef _DEBUG

// Internal helper class for scope-based benchmarks with 1-second averaging
class BenchmarkScope {
private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    TimePoint m_start;
    const char* m_pName;

    struct BenchmarkSlot {
        const char* m_name;
        int64_t m_accumulated_ns;
        int m_call_count;
        TimePoint m_last_output_time;
    };
    static BenchmarkSlot s_slots[64];
    static int s_iSlotCount;

    int find_or_create_slot(const char* name) {
        for (int i = 0; i < s_iSlotCount; i++) {
            if (strcmp(s_slots[i].m_name, name) == 0) {
                return i;
            }
        }

        if (s_iSlotCount < 64) {
            int idx = s_iSlotCount++;
            s_slots[idx].m_name = name;
            s_slots[idx].m_accumulated_ns = 0;
            s_slots[idx].m_call_count = 0;
            s_slots[idx].m_last_output_time = Clock::now();
            return idx;
        }
        return -1;
    }

public:
    BenchmarkScope(const char* name)
        : m_pName(name)
        , m_start(Clock::now())
    {
    }

    ~BenchmarkScope()
    {
        auto end = Clock::now();
        int64_t elapsed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();

        int slot = find_or_create_slot(m_pName);
        if (slot < 0) return;

        s_slots[slot].m_accumulated_ns += elapsed_ns;
        s_slots[slot].m_call_count++;

        auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(end - s_slots[slot].m_last_output_time).count();
        if (since_last >= 1000) {
            double avg_ms = (s_slots[slot].m_accumulated_ns / 1000000.0) / s_slots[slot].m_call_count;
            double avg_us = (s_slots[slot].m_accumulated_ns / 1000.0) / s_slots[slot].m_call_count;
            double total_ms = s_slots[slot].m_accumulated_ns / 1000000.0;

            if (avg_ms >= 1.0) {
                printf("[BENCHMARK] %s: %.3f ms/call (%d calls, %.1f ms total in 1s)\n",
                         m_pName, avg_ms, s_slots[slot].m_call_count, total_ms);
            } else {
                printf("[BENCHMARK] %s: %.1f us/call (%d calls, %.3f ms total in 1s)\n",
                         m_pName, avg_us, s_slots[slot].m_call_count, total_ms);
            }

            s_slots[slot].m_accumulated_ns = 0;
            s_slots[slot].m_call_count = 0;
            s_slots[slot].m_last_output_time = end;
        }
    }
};

// Static members initialization
inline BenchmarkScope::BenchmarkSlot BenchmarkScope::s_slots[64];
inline int BenchmarkScope::s_iSlotCount = 0;

// Two-level indirection so __LINE__ expands before token-pasting
#define BENCHMARK_CONCAT_IMPL_(a,b) a##b
#define BENCHMARK_CONCAT_(a,b) BENCHMARK_CONCAT_IMPL_(a,b)

// Scope-based benchmark (RAII - automatic timing)
#define BENCHMARK_SCOPE(name) \
    BenchmarkScope BENCHMARK_CONCAT_(__benchmark_,__LINE__)(name)

// Manual start/end benchmarks
#define BENCHMARK_START(var) \
    auto __bench_start_##var = std::chrono::high_resolution_clock::now();

#define BENCHMARK_END(var, name) \
    { \
        auto __bench_end_##var = std::chrono::high_resolution_clock::now(); \
        double __ms_##var = std::chrono::duration<double, std::milli>(__bench_end_##var - __bench_start_##var).count(); \
        double __us_##var = __ms_##var * 1000.0; \
        if (__ms_##var >= 1.0) { \
            printf("[BENCHMARK] %s: %.3f ms\n", name, __ms_##var); \
        } else { \
            printf("[BENCHMARK] %s: %.1f us\n", name, __us_##var); \
        } \
    }

// Quick one-liner for measuring a single statement
#define BENCHMARK_STMT(name, stmt) \
    { \
        BENCHMARK_SCOPE(name); \
        stmt; \
    }

#else
// Release mode - all benchmarks compile to nothing
#define BENCHMARK_SCOPE(name)
#define BENCHMARK_START(var)
#define BENCHMARK_END(var, name)
#define BENCHMARK_STMT(name, stmt) stmt
#endif
