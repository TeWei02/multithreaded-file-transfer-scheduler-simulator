CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wpedantic -Iinclude
LDFLAGS ?= -pthread

TARGET := simulator
TEST_TARGET := simulator_tests
CORE_SRC := \
	src/request.cpp \
	src/scheduler.cpp \
	src/worker_manager.cpp \
	src/csv_utils.cpp \
	src/metrics.cpp \
	src/simulation_engine.cpp
SRC := src/main.cpp $(CORE_SRC)
TEST_SRC := tests/test_simulator.cpp $(CORE_SRC)

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

$(TEST_TARGET): $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) -o $(TEST_TARGET) $(LDFLAGS)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)

.PHONY: all test clean
