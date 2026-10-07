CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic
CXXFLAGS += -Iinclude

# raylib flags: pkg-config when available, plain -lraylib otherwise.
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null || echo -lraylib -lm)

BIN     := bin
HEADERS := $(wildcard include/*.h)

.PHONY: all check clean

all: $(BIN)/checker $(BIN)/viz $(BIN)/demo

$(BIN):
	mkdir -p $(BIN)

# No raylib needed for the checker.
$(BIN)/checker: src/checker.cpp $(HEADERS) | $(BIN)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BIN)/viz: src/viz.cpp $(HEADERS) | $(BIN)
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) $< -o $@ $(RAYLIB_LIBS)

$(BIN)/demo: src/demo.cpp $(HEADERS) | $(BIN)
	$(CXX) $(CXXFLAGS) $(RAYLIB_CFLAGS) $< -o $@ $(RAYLIB_LIBS)

check: $(BIN)/checker
	./$(BIN)/checker

clean:
	rm -rf $(BIN)
