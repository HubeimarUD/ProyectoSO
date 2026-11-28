#ifndef LOGPULSE_PARSER_H
#define LOGPULSE_PARSER_H

#include <string>
#include <string_view>

namespace logpulse {

struct ParsedLogEntry {
    bool valid = false;
    bool has_request_time = false;
    std::string ip;
    std::string timestamp;
    std::string method;
    std::string url_path; // URL sin query string (?...)
    int status_code = 0;
    long long bytes = 0;
    double request_time = 0.0;
};

// Parsea una linea de log en formato combinado + tiempo de respuesta opcional.
// Nunca lanza excepciones ni detiene el programa ante lineas mal formadas.
ParsedLogEntry parse_log_line(std::string_view line);

// Elimina la cadena de consulta ('?') si existe en una URL.
std::string strip_query_string(std::string_view raw_url);

} // namespace logpulse

#endif
