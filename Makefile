CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -pthread -Iinclude
LDFLAGS = -pthread -lncurses

BIN_DIR = build
LOG_DIR = logs
LOGPULSE = $(BIN_DIR)/logpulse
LOGGEN = $(BIN_DIR)/loggen
SAMPLE_LOG = $(LOG_DIR)/access_test.log

.PHONY: all clean run gen

all: $(LOGGEN) $(LOGPULSE)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(LOG_DIR):
	mkdir -p $(LOG_DIR)

$(LOGGEN): src/loggen.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(LOGPULSE): src/main_stub.cpp src/parser.cpp src/metrics.cpp | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

gen: $(LOGGEN) | $(LOG_DIR)
	./$(LOGGEN) -n 1000000 -o $(SAMPLE_LOG)

run: $(LOGPULSE)
	./$(LOGPULSE)

clean:
	rm -rf $(BIN_DIR) $(LOG_DIR)
