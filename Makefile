# cpp-polycall -- GNU Make build (Linux / MSYS2). The installed Polycall core
# is found with pkg-config (PKG_CONFIG_PATH=<prefix>/lib/pkgconfig); CMake
# users use CMakeLists.txt (find_package(polycall)).
CXX ?= g++
AR ?= ar
PKG_CONFIG ?= pkg-config

POLYCALL_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags polycall 2>/dev/null)
POLYCALL_LIBS ?= $(shell $(PKG_CONFIG) --libs polycall 2>/dev/null)

CPPFLAGS ?=
CPPFLAGS += -Iinclude $(POLYCALL_CFLAGS)
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wpedantic
LDLIBS += $(POLYCALL_LIBS) -pthread

BUILD_DIR := build
LIB_DIR := lib
ADAPTER_OBJ := $(BUILD_DIR)/polycall.o
STATIC_LIB := $(LIB_DIR)/libcpp_polycall.a
TEST_BIN := $(BUILD_DIR)/cpp_polycall_test
EXAMPLE_BIN := $(BUILD_DIR)/cpp-polycall

ifeq ($(OS),Windows_NT)
EXE_EXT := .exe
TEST_BIN := $(TEST_BIN)$(EXE_EXT)
EXAMPLE_BIN := $(EXAMPLE_BIN)$(EXE_EXT)
endif

.DEFAULT_GOAL := all

.PHONY: all
all: check-core $(STATIC_LIB)

.PHONY: check-core
check-core:
	@test -n "$(POLYCALL_LIBS)" || { echo "cpp-polycall: pkg-config cannot find polycall (>= 1.1.0); set PKG_CONFIG_PATH=<prefix>/lib/pkgconfig" >&2; exit 2; }

$(BUILD_DIR) $(LIB_DIR):
	@mkdir -p $@

$(ADAPTER_OBJ): src/polycall.cpp include/cpp_polycall/polycall.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(STATIC_LIB): $(ADAPTER_OBJ) | $(LIB_DIR)
	$(AR) rcs $@ $^

$(TEST_BIN): tests/real_core_test.cpp $(ADAPTER_OBJ) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# Real-core test incl. interop with the C CLI (SKIP, exit 77, without it).
.PHONY: test
test: check-core $(TEST_BIN)
	sh tests/run-real.sh $(TEST_BIN) .

.PHONY: example
example: check-core $(ADAPTER_OBJ) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) examples/main.cpp $(ADAPTER_OBJ) -o $(EXAMPLE_BIN) $(LDFLAGS) $(LDLIBS)
	$(EXAMPLE_BIN)

.PHONY: verify-dry
verify-dry:
	sh scripts/verify-dry.sh

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(LIB_DIR) cmake-build

-include $(ADAPTER_OBJ:.o=.d)
