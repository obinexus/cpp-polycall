#include "polycall_ffi_mock.hpp"

#include <string>

namespace {
int mock_status;
int mock_calls;
int mock_run;
std::string mock_config;
} // namespace

extern "C" int polycall_ffi_run_config(const char* config_path, int run) {
    ++mock_calls;
    mock_run = run;
    mock_config = config_path ? config_path : "";
    return mock_status;
}

void polycall_ffi_mock_reset() {
    mock_status = 0;
    mock_calls = 0;
    mock_run = 0;
    mock_config.clear();
}

void polycall_ffi_mock_return_status(int status) {
    mock_status = status;
}

int polycall_ffi_mock_call_count() {
    return mock_calls;
}

int polycall_ffi_mock_last_run() {
    return mock_run;
}

const char* polycall_ffi_mock_last_config() {
    return mock_config.c_str();
}
