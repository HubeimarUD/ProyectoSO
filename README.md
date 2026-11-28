# LogPulse - Analizador de Logs en Tiempo Real (Proyecto Final SO)

Monitor y analizador concurrente de logs de servidores web (formato combinado Apache/Nginx) desarrollado en **C++17** bajo Linux.

## Estado actual: Etapa 1 completada

Los modulos base de generacion, parseo y agregacion de metricas ya estan implementados y verificados sin fugas de memoria (valgrind):

- src/loggen.cpp: Generador independiente de trafico simulado en formato combinado (IP - - [fecha] "METODO URL HTTP/1.1" status bytes "User-Agent" tiempo_respuesta).
  - Opciones: -o <archivo>, -n <lineas>, -r <tasa_por_seg>, --rotar.
- include/parser.h y src/parser.cpp: Parseador tolerante a fallos (logpulse::parse_log_line).
  - Extrae IP, timestamp, metodo, ruta limpia (sin query string ?param=...), codigo HTTP, bytes y tiempo de respuesta opcional.
  - Descarta lineas mal formadas marcando valid = false sin interrumpir la ejecucion.
- include/metrics.h y src/metrics.cpp: Colector y estructuras de metricas (logpulse::MetricsCollector, logpulse::MetricsSnapshot).
  - Contabiliza peticiones totales, RPS, distribucion 2xx/3xx/4xx/5xx, bytes transferidos, lineas mal formadas y rankings Top-N (IPs, URLs mas solicitadas y URLs mas lentas).
  - Incluye merge_from() para combinar resultados parciales de multiples trabajadores.
- src/main_stub.cpp: Programa temporal de verificacion secuencial sobre logs/access_test.log.

## Compilacion y pruebas rapidas

    make        # Compila binarios (build/loggen y build/logpulse)
    make gen    # Genera archivo de prueba de 1,000,000 de lineas en logs/access_test.log
    make run    # Ejecuta prueba actual de parseo y metricas

## Guia para las siguientes etapas

1. Etapa 2 (Lectura y paralelismo / concurrencia):
   - Reemplazar src/main_stub.cpp por el flujo principal del monitor (lectura continua y manejo de rotacion de logs).
   - Distribuir lineas entre hilos o procesos trabajadores usando logpulse::parse_log_line() y un MetricsCollector por trabajador, consolidando luego con merge_from().
2. Etapa 3 (IPC / Memoria compartida y sincronizacion):
   - Exponer el resumen de MetricsSnapshot mediante memoria compartida (shm_open + mmap) y mecanismos de sincronizacion POSIX (sem_open / pthread_mutex_t) segun la arquitectura del equipo.
3. Etapa 4 (Interfaz TUI en terminal y senales):
   - Construir la visualizacion interactiva en tiempo real usando ncurses (ya enlazado con -lncurses en el Makefile) y manejar cierre limpio ante SIGINT/SIGTERM.
