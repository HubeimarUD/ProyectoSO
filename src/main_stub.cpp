#include "metrics.h"
#include "parser.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main() {
    std::ifstream in("logs/access_test.log");
    if (!in.is_open()) {
        std::cerr << "No se pudo abrir logs/access_test.log\n";
        return 1;
    }

    logpulse::MetricsCollector collector;
    std::string line;
    auto t0 = std::chrono::steady_clock::now();

    while (std::getline(in, line)) {
        auto parsed = logpulse::parse_log_line(line);
        if (parsed.valid) {
            collector.record_valid_entry(parsed);
        } else {
            collector.record_malformed_line();
        }
    }

    auto t1 = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t1 - t0).count();
    auto snap = collector.snapshot(elapsed, 10);

    std::cout << "Total peticiones: " << snap.total_requests
              << " | Malformadas: " << snap.malformed_lines
              << " | RPS: " << std::fixed << std::setprecision(1) << snap.requests_per_second
              << " | Bytes: " << snap.total_bytes << "\n";
    std::cout << "2xx=" << snap.status_2xx
              << " 3xx=" << snap.status_3xx
              << " 4xx=" << snap.status_4xx
              << " 5xx=" << snap.status_5xx << "\n";

    std::cout << "Top URLs:\n";
    for (const auto& u : snap.top_urls) {
        std::cout << "  " << u.key << " -> " << u.count << "\n";
    }

    std::cout << "Top URLs lentas (promedio seg):\n";
    for (const auto& s : snap.slowest_urls) {
        std::cout << "  " << s.url << " -> " << std::setprecision(4) << s.avg_time << "\n";
    }

    return 0;
}
