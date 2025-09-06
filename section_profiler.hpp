#ifndef SECTION_PROFILER_HPP
#define SECTION_PROFILER_HPP

#include <chrono>
#include <iostream>
#include <iomanip>
#include <unordered_map>
#include <mutex>
#include <string>
#include <stack>
#include <thread>
#include <cmath>
#include <algorithm>

class SectionProfiler {
  public:
    SectionProfiler(const std::string &section_name)
        : name(section_name), start(std::chrono::high_resolution_clock::now()) {
        // Determine parent stats
        parent_stats = active_stack().empty() ? nullptr : active_stack().top()->current_stats;

        // Create or get this section's stats under parent
        if (parent_stats) {
            current_stats = &parent_stats->children[name];
        } else {
            current_stats = &get_root_stats()[name];
        }

        active_stack().push(this);
    }

    ~SectionProfiler() {
        auto end = std::chrono::high_resolution_clock::now();
        double duration = std::chrono::duration<double, std::milli>(end - start).count();

        std::lock_guard<std::mutex> lock(get_mutex());

        // update stats
        current_stats->total_time += duration;
        current_stats->call_count++;
        current_stats->min_time = std::min(current_stats->min_time, duration);
        current_stats->max_time = std::max(current_stats->max_time, duration);
        current_stats->sum_squares += duration * duration;

        active_stack().pop();
    }

    static void report() {
        std::lock_guard<std::mutex> lock(get_mutex());
        std::cout << "\n=== Profiling Report ===\n";
        print_stats(get_root_stats(), "", 0.0);
        std::cout << "========================\n";
    }

  private:
    struct Stats {
        double total_time = 0.0;
        size_t call_count = 0;
        double min_time = std::numeric_limits<double>::infinity();
        double max_time = 0.0;
        double sum_squares = 0.0;
        std::unordered_map<std::string, Stats> children;
    };

    std::string name;
    std::chrono::high_resolution_clock::time_point start;
    Stats *parent_stats = nullptr;  // pointer to parent section stats
    Stats *current_stats = nullptr; // pointer to this section's stats

    static std::unordered_map<std::string, Stats> &get_root_stats() {
        static std::unordered_map<std::string, Stats> root;
        return root;
    }

    static std::mutex &get_mutex() {
        static std::mutex m;
        return m;
    }

    static std::stack<SectionProfiler *> &active_stack() {
        thread_local std::stack<SectionProfiler *> stack;
        return stack;
    }

    static void print_stats(const std::unordered_map<std::string, Stats> &stats_map, const std::string &indent,
                            double parent_time) {
        for (const auto &[name, stats] : stats_map) {
            double avg = stats.call_count ? stats.total_time / stats.call_count : 0.0;
            double variance = stats.call_count ? stats.sum_squares / stats.call_count - avg * avg : 0.0;
            double stddev = std::sqrt(std::max(0.0, variance));
            double pct = parent_time > 0.0 ? (stats.total_time / parent_time) * 100.0 : 100.0;

            std::cout << indent << std::left << std::setw(30) << name << std::right << "  Total: " << std::setw(10)
                      << stats.total_time << " ms  Avg: " << std::setw(8) << avg << " ms  Min: " << std::setw(8)
                      << (stats.min_time == std::numeric_limits<double>::infinity() ? 0.0 : stats.min_time)
                      << " ms  Max: " << std::setw(8) << stats.max_time << " ms  StdDev: " << std::setw(8) << stddev
                      << " ms  " << std::setw(5) << pct << "% of parent"
                      << "\n";

            if (!stats.children.empty()) {
                print_stats(stats.children, indent + "  ", stats.total_time);
            }
        }
    }
};

// Macro helpers
#define PROFILE_SECTION_INTERNAL2(name, line) SectionProfiler profiler_##line(name)
#define PROFILE_SECTION_INTERNAL(name, line) PROFILE_SECTION_INTERNAL2(name, line)

// Default: use function name
#define PROFILE_SECTION(...)                                                                                           \
    PROFILE_SECTION_INTERNAL(sizeof(#__VA_ARGS__) == 1 ? std::string(__FUNCTION__) : std::string(__VA_ARGS__), __LINE__)

#endif // SECTION_PROFILER_HPP
