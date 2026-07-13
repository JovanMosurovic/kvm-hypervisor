CC       = gcc
CXX      = g++
CPPFLAGS = -Iinc -I../common
CFLAGS   = -Wall -Wextra
CXXFLAGS = -std=c++17 -Wall -Wextra
LDLIBS   = -pthread

BUILD_DIR ?= build

C_SRCS   = $(wildcard src/*.c)
CXX_SRCS = $(wildcard src/*.cpp)
C_OBJS   = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SRCS))
CXX_OBJS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(CXX_SRCS))
OBJS     = $(C_OBJS) $(CXX_OBJS)
DEPS = $(OBJS:.o=.d)

BIN = $(BUILD_DIR)/hypervisor

.PHONY: all clean

all: $(BUILD_DIR) $(BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c -o $@ $<

$(BIN): $(OBJS)
	$(CXX) $(OBJS) $(LDLIBS) -o $@

clean:
	rm -rf $(BUILD_DIR)

-include $(DEPS)
