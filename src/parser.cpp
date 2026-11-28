#include "parser.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <string_view>

namespace logpulse {

namespace {

std::string_view trim_view(std::string_view sv) {
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.front()))) {
        sv.remove_prefix(1);
    }
    while (!sv.empty() && std::isspace(static_cast<unsigned char>(sv.back()))) {
        sv.remove_suffix(1);
    }
    return sv;
}

bool parse_int_strict(std::string_view sv, int& out) {
    sv = trim_view(sv);
    if (sv.empty()) {
        return false;
    }
    for (char c : sv) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    std::string tmp(sv);
    char* end = nullptr;
    long val = std::strtol(tmp.c_str(), &end, 10);
    if (end == tmp.c_str() || *end != '\0' || val < 100 || val > 599) {
        return false;
    }
    out = static_cast<int>(val);
    return true;
}

bool parse_bytes_field(std::string_view sv, long long& out) {
    sv = trim_view(sv);
    if (sv.empty()) {
        return false;
    }
    if (sv == "-") {
        out = 0;
        return true;
    }
    for (char c : sv) {
        if (!std::isdigit(static_cast<unsigned char>(c))) {
            return false;
        }
    }
    std::string tmp(sv);
    char* end = nullptr;
    long long val = std::strtoll(tmp.c_str(), &end, 10);
    if (end == tmp.c_str() || *end != '\0' || val < 0) {
        return false;
    }
    out = val;
    return true;
}

bool parse_double_strict(std::string_view sv, double& out) {
    sv = trim_view(sv);
    if (sv.empty() || sv == "-") {
        return false;
    }
    std::string tmp(sv);
    char* end = nullptr;
    double val = std::strtod(tmp.c_str(), &end);
    if (end == tmp.c_str() || *end != '\0' || val < 0.0) {
        return false;
    }
    out = val;
    return true;
}

} // namespace

std::string strip_query_string(std::string_view raw_url) {
    size_t qpos = raw_url.find('?');
    if (qpos != std::string_view::npos) {
        raw_url = raw_url.substr(0, qpos);
    }
    return std::string(raw_url);
}

ParsedLogEntry parse_log_line(std::string_view line) {
    ParsedLogEntry entry;
    line = trim_view(line);
    if (line.empty()) {
        return entry;
    }

    // 1. IP al inicio
    size_t sp1 = line.find(' ');
    if (sp1 == std::string_view::npos || sp1 == 0) {
        return entry;
    }
    std::string_view ip_sv = line.substr(0, sp1);

    // 2. Fecha y hora entre '[' y ']'
    size_t lb = line.find('[', sp1 + 1);
    size_t rb = (lb != std::string_view::npos) ? line.find(']', lb + 1) : std::string_view::npos;
    if (lb == std::string_view::npos || rb == std::string_view::npos || rb <= lb + 1) {
        return entry;
    }
    std::string_view ts_sv = line.substr(lb + 1, rb - lb - 1);

    // 3. Primera seccion entre comillas: "METODO URL PROTOCOLO"
    size_t q1_start = line.find('"', rb + 1);
    size_t q1_end = (q1_start != std::string_view::npos) ? line.find('"', q1_start + 1) : std::string_view::npos;
    if (q1_start == std::string_view::npos || q1_end == std::string_view::npos || q1_end <= q1_start + 1) {
        return entry;
    }
    std::string_view req_sv = trim_view(line.substr(q1_start + 1, q1_end - q1_start - 1));

    size_t m_sp = req_sv.find(' ');
    if (m_sp == std::string_view::npos || m_sp == 0) {
        return entry;
    }
    std::string_view method_sv = req_sv.substr(0, m_sp);

    std::string_view rest_req = trim_view(req_sv.substr(m_sp + 1));
    if (rest_req.empty()) {
        return entry;
    }
    size_t u_sp = rest_req.find(' ');
    std::string_view url_sv = (u_sp == std::string_view::npos) ? rest_req : rest_req.substr(0, u_sp);
    if (url_sv.empty()) {
        return entry;
    }

    // 4. Despues de la primera comilla: status, bytes, luego "User-Agent" y opcional request_time
    std::string_view after_req = trim_view(line.substr(q1_end + 1));
    size_t st_sp = after_req.find(' ');
    if (st_sp == std::string_view::npos) {
        return entry;
    }
    std::string_view status_sv = after_req.substr(0, st_sp);
    int status_val = 0;
    if (!parse_int_strict(status_sv, status_val)) {
        return entry;
    }

    std::string_view after_status = trim_view(after_req.substr(st_sp + 1));
    size_t b_sp = after_status.find(' ');
    if (b_sp == std::string_view::npos) {
        return entry;
    }
    std::string_view bytes_sv = after_status.substr(0, b_sp);
    long long bytes_val = 0;
    if (!parse_bytes_field(bytes_sv, bytes_val)) {
        return entry;
    }

    std::string_view after_bytes = trim_view(after_status.substr(b_sp + 1));
    // Referer opcional antes del User-Agent si existe
    if (!after_bytes.empty() && after_bytes.front() == '"') {
        size_t first_q_end = after_bytes.find('"', 1);
        if (first_q_end == std::string_view::npos) {
            return entry;
        }
        std::string_view after_first_q = trim_view(after_bytes.substr(first_q_end + 1));
        if (!after_first_q.empty() && after_first_q.front() == '"') {
            // Habia referer y ahora viene user-agent
            size_t ua_end = after_first_q.find('"', 1);
            if (ua_end == std::string_view::npos) {
                return entry;
            }
            std::string_view tail_sv = trim_view(after_first_q.substr(ua_end + 1));
            if (!tail_sv.empty()) {
                double rt = 0.0;
                if (parse_double_strict(tail_sv, rt)) {
                    entry.request_time = rt;
                    entry.has_request_time = true;
                }
            }
        } else {
            // Solo habia user-agent entre comillas
            if (!after_first_q.empty()) {
                double rt = 0.0;
                if (parse_double_strict(after_first_q, rt)) {
                    entry.request_time = rt;
                    entry.has_request_time = true;
                }
            }
        }
    }

    entry.ip = std::string(ip_sv);
    entry.timestamp = std::string(ts_sv);
    entry.method = std::string(method_sv);
    entry.url_path = strip_query_string(url_sv);
    entry.status_code = status_val;
    entry.bytes = bytes_val;
    entry.valid = true;
    return entry;
}

} // namespace logpulse
