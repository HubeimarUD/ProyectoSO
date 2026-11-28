#include "metrics.h"

#include <algorithm>

namespace logpulse {

void MetricsCollector::record_valid_entry(const ParsedLogEntry& entry) {
    if (!entry.valid) {
        record_malformed_line(1);
        return;
    }

    ++total_requests_;
    if (entry.bytes > 0) {
        total_bytes_ += static_cast<uint64_t>(entry.bytes);
    }

    if (entry.status_code >= 200 && entry.status_code < 300) {
        ++status_2xx_;
    } else if (entry.status_code >= 300 && entry.status_code < 400) {
        ++status_3xx_;
    } else if (entry.status_code >= 400 && entry.status_code < 500) {
        ++status_4xx_;
    } else if (entry.status_code >= 500 && entry.status_code < 600) {
        ++status_5xx_;
    } else {
        ++status_other_;
    }

    if (!entry.ip.empty()) {
        ++ip_counts_[entry.ip];
    }

    if (!entry.url_path.empty()) {
        UrlStats& u = url_stats_[entry.url_path];
        ++u.hits;
        if (entry.has_request_time && entry.request_time >= 0.0) {
            ++u.timed_hits;
            u.total_response_time += entry.request_time;
        }
    }
}

void MetricsCollector::record_malformed_line(uint64_t count) {
    malformed_lines_ += count;
}

void MetricsCollector::merge_from(const MetricsCollector& other) {
    total_requests_ += other.total_requests_;
    malformed_lines_ += other.malformed_lines_;
    status_2xx_ += other.status_2xx_;
    status_3xx_ += other.status_3xx_;
    status_4xx_ += other.status_4xx_;
    status_5xx_ += other.status_5xx_;
    status_other_ += other.status_other_;
    total_bytes_ += other.total_bytes_;

    for (const auto& [ip, cnt] : other.ip_counts_) {
        ip_counts_[ip] += cnt;
    }

    for (const auto& [url, stats] : other.url_stats_) {
        UrlStats& dst = url_stats_[url];
        dst.hits += stats.hits;
        dst.timed_hits += stats.timed_hits;
        dst.total_response_time += stats.total_response_time;
    }
}

void MetricsCollector::reset() {
    total_requests_ = 0;
    malformed_lines_ = 0;
    status_2xx_ = 0;
    status_3xx_ = 0;
    status_4xx_ = 0;
    status_5xx_ = 0;
    status_other_ = 0;
    total_bytes_ = 0;
    ip_counts_.clear();
    url_stats_.clear();
}

MetricsSnapshot MetricsCollector::snapshot(double elapsed_seconds, size_t top_n) const {
    MetricsSnapshot snap;
    snap.total_requests = total_requests_;
    snap.malformed_lines = malformed_lines_;
    snap.status_2xx = status_2xx_;
    snap.status_3xx = status_3xx_;
    snap.status_4xx = status_4xx_;
    snap.status_5xx = status_5xx_;
    snap.status_other = status_other_;
    snap.total_bytes = total_bytes_;
    snap.elapsed_seconds = (elapsed_seconds > 0.0) ? elapsed_seconds : 0.0;
    snap.requests_per_second =
        (snap.elapsed_seconds > 0.0)
            ? static_cast<double>(total_requests_) / snap.elapsed_seconds
            : 0.0;

    std::vector<RankedCount> ips_vec;
    ips_vec.reserve(ip_counts_.size());
    for (const auto& [ip, cnt] : ip_counts_) {
        ips_vec.push_back(RankedCount{ip, cnt});
    }
    std::sort(ips_vec.begin(), ips_vec.end(), [](const RankedCount& a, const RankedCount& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.key < b.key;
    });
    if (ips_vec.size() > top_n) {
        ips_vec.resize(top_n);
    }
    snap.top_ips = std::move(ips_vec);

    std::vector<RankedCount> urls_vec;
    urls_vec.reserve(url_stats_.size());
    std::vector<RankedAvgTime> slow_vec;
    slow_vec.reserve(url_stats_.size());

    for (const auto& [url, stats] : url_stats_) {
        urls_vec.push_back(RankedCount{url, stats.hits});
        if (stats.timed_hits > 0) {
            double avg = stats.total_response_time / static_cast<double>(stats.timed_hits);
            slow_vec.push_back(RankedAvgTime{url, stats.timed_hits, avg});
        }
    }

    std::sort(urls_vec.begin(), urls_vec.end(), [](const RankedCount& a, const RankedCount& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.key < b.key;
    });
    if (urls_vec.size() > top_n) {
        urls_vec.resize(top_n);
    }
    snap.top_urls = std::move(urls_vec);

    std::sort(slow_vec.begin(), slow_vec.end(), [](const RankedAvgTime& a, const RankedAvgTime& b) {
        if (a.avg_time != b.avg_time) return a.avg_time > b.avg_time;
        return a.url < b.url;
    });
    if (slow_vec.size() > top_n) {
        slow_vec.resize(top_n);
    }
    snap.slowest_urls = std::move(slow_vec);

    return snap;
}

} // namespace logpulse
