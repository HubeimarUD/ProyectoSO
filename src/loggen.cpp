#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {
std::atomic<bool> g_running{true};

void handle_signal(int) {
    g_running.store(false);
}

std::string format_apache_time() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm_local{};
    localtime_r(&t, &tm_local);

    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%d/%b/%Y:%H:%M:%S", &tm_local);

    char tz_buf[16];
    std::strftime(tz_buf, sizeof(tz_buf), "%z", &tm_local);

    std::ostringstream oss;
    oss << "[" << time_buf << " " << tz_buf << "]";
    return oss.str();
}

void rotate_file(const std::string& path, std::ofstream& out) {
    out.flush();
    out.close();

    std::error_code ec;
    if (fs::exists(path, ec)) {
        std::string rotated = path + ".1";
        fs::remove(rotated, ec);
        fs::rename(path, rotated, ec);
    }

    out.open(path, std::ios::out | std::ios::app);
}

void print_usage(const char* prog) {
    std::cerr << "Uso: " << prog
              << " [-o archivo] [-n lineas] [-r tasa_por_seg] [--rotar]\n";
}
} // namespace

int main(int argc, char* argv[]) {
    std::string output_path = "logs/access_test.log";
    long long num_lines = -1;
    double rate_per_sec = 0.0;
    bool do_rotate = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "-n" && i + 1 < argc) {
            num_lines = std::stoll(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            rate_per_sec = std::stod(argv[++i]);
        } else if (arg == "--rotar") {
            do_rotate = true;
        } else if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    fs::path p(output_path);
    if (p.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(p.parent_path(), ec);
    }

    std::ofstream out(output_path, std::ios::out | std::ios::app);
    if (!out.is_open()) {
        std::cerr << "Error al abrir el archivo de salida: " << output_path << "\n";
        return 1;
    }

    if (do_rotate) {
        rotate_file(output_path, out);
        if (!out.is_open()) {
            std::cerr << "Error al reabrir el archivo tras rotar: " << output_path << "\n";
            return 1;
        }
        std::cout << "Rotacion completada para " << output_path << "\n";
        if (num_lines <= 0 && rate_per_sec <= 0.0) {
            return 0;
        }
    }

    const std::vector<std::string> ips = {
        "192.168.10.24", "192.168.10.55", "10.0.0.12", "172.16.4.89",
        "181.54.120.3", "190.144.22.18", "200.21.88.101", "10.0.1.77"
    };
    const std::vector<std::string> methods = {"GET", "POST", "PUT", "DELETE"};
    const std::vector<std::string> urls = {
        "/api/notas?id=42", "/login", "/dashboard", "/api/usuarios",
        "/static/app.js", "/api/reportes?mes=11", "/health", "/api/cursos?sem=2026-2"
    };
    const std::vector<int> statuses = {200, 200, 200, 201, 204, 302, 400, 401, 404, 500, 502};
    const std::vector<std::string> agents = {
        "Mozilla/5.0", "curl/8.5.0", "Mozilla/5.0 (X11; Linux x86_64)", "PostmanRuntime/7.39.0"
    };

    std::mt19937 rng(42);
    std::uniform_int_distribution<size_t> ip_dist(0, ips.size() - 1);
    std::uniform_int_distribution<size_t> method_dist(0, methods.size() - 1);
    std::uniform_int_distribution<size_t> url_dist(0, urls.size() - 1);
    std::uniform_int_distribution<size_t> status_dist(0, statuses.size() - 1);
    std::uniform_int_distribution<size_t> agent_dist(0, agents.size() - 1);
    std::uniform_int_distribution<int> bytes_dist(120, 18500);
    std::uniform_real_distribution<double> rt_dist(0.002, 1.450);

    std::string ts = format_apache_time();
    auto last_ts_update = std::chrono::steady_clock::now();

    long long written = 0;
    auto start_time = std::chrono::steady_clock::now();

    while (g_running.load()) {
        if (num_lines > 0 && written >= num_lines) {
            break;
        }

        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - last_ts_update).count() >= 1) {
            ts = format_apache_time();
            last_ts_update = now;
        }

        int status = statuses[status_dist(rng)];
        int bytes = (status == 204 || status == 304) ? 0 : bytes_dist(rng);
        double req_time = rt_dist(rng);

        out << ips[ip_dist(rng)] << " - - " << ts << " \""
            << methods[method_dist(rng)] << " " << urls[url_dist(rng)] << " HTTP/1.1\" "
            << status << " " << bytes << " \""
            << agents[agent_dist(rng)] << "\" "
            << std::fixed << std::setprecision(3) << req_time << "\n";

        ++written;

        if (rate_per_sec > 0.0) {
            double expected_elapsed = static_cast<double>(written) / rate_per_sec;
            auto current = std::chrono::steady_clock::now();
            double actual_elapsed = std::chrono::duration<double>(current - start_time).count();
            if (expected_elapsed > actual_elapsed) {
                auto sleep_dur = std::chrono::duration<double>(expected_elapsed - actual_elapsed);
                std::this_thread::sleep_for(sleep_dur);
            }
        }
    }

    out.flush();
    out.close();
    return 0;
}
