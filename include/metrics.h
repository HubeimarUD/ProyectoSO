#ifndef LOGPULSE_METRICS_H
#define LOGPULSE_METRICS_H

#include "parser.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace logpulse {

struct RankedCount {
    std::string key;
    uint64_t count = 0;
};

struct RankedAvgTime {
    std::string url;
    uint64_t timed_count = 0;
    double avg_time = 0.0;
};

struct MetricsSnapshot {
    uint64_t total_requests = 0;
    uint64_t malformed_lines = 0;
    uint64_t status_2xx = 0;
    uint64_t status_3xx = 0;
    uint64_t status_4xx = 0;
    uint64_t status_5xx = 0;
    uint64_t status_other = 0;
    uint64_t total_bytes = 0;
    double elapsed_seconds = 0.0;
    double requests_per_second = 0.0;

    std::vector<RankedCount> top_ips;
    std::vector<RankedCount> top_urls;
    std::vector<RankedAvgTime> slowest_urls;
};

class MetricsCollector {
public:
    void record_valid_entry(const ParsedLogEntry& entry);
    void record_malformed_line(uint64_t count = 1);
    void merge_from(const MetricsCollector& other);
    void reset();

    MetricsSnapshot snapshot(double elapsed_seconds, size_t top_n = 10) const;

private:
    struct UrlStats {
        uint64_t hits = 0;
        uint64_t timed_hits = 0;
        double total_response_time = 0.0;
    };

    uint64_t total_requests_ = 0;
    uint64_t malformed_lines_ = 0;
    uint64_t status_2xx_ = 0;
    uint64_t status_3xx_ = 0;
    uint64_t status_4xx_ = 0;
    uint64_t status_5xx_ = 0;
    uint64_t status_other_ = 0;
    uint64_t total_bytes_ = 0;

    std::unordered_map<std::string, uint64_t> ip_counts_;
    std::unordered_map<std::string, UrlStats> url_stats_;
};

} // namespace logpulse

#endif
