#pragma once

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace benchmark {

class LatencyStats {
public:
    using Duration = std::chrono::nanoseconds;

    void add(Duration sample) {
        samples_.push_back(sample);
        if (sample < min_) {
            min_ = sample;
        }
        if (sample > max_) {
            max_ = sample;
        }
        total_ += sample;
    }

    [[nodiscard]] bool empty() const {
        return samples_.empty();
    }

    void print_summary(const std::string& label) const {
        if (samples_.empty()) {
            std::cout << label << ": no samples collected\n";
            return;
        }

        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.end());

        std::cout << std::fixed << std::setprecision(3);
        std::cout << label << " samples=" << samples_.size()
                  << " min_us=" << to_microseconds(min_)
                  << " avg_us=" << to_microseconds(total_ / samples_.size())
                  << " p50_us=" << to_microseconds(percentile(sorted, 0.50))
                  << " p95_us=" << to_microseconds(percentile(sorted, 0.95))
                  << " max_us=" << to_microseconds(max_) << "\n";
    }

private:
    static double to_microseconds(Duration duration) {
        return std::chrono::duration<double, std::micro>(duration).count();
    }

    static Duration percentile(const std::vector<Duration>& sorted, double ratio) {
        const auto index = static_cast<std::size_t>(
            ratio * static_cast<double>(sorted.size() - 1U)
        );
        return sorted[index];
    }

    std::vector<Duration> samples_;
    Duration min_ = Duration::max();
    Duration max_ = Duration::zero();
    Duration total_ = Duration::zero();
};

} // namespace benchmark
