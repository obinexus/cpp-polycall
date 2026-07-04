#include "cpp_polycall/polycall.hpp"
#include "polycall_ffi_mock.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    const std::string config_path = "cpp-polycallrc";

    polycall_ffi_mock_reset();
    assert(polycall::run_config(config_path) == 0);
    assert(polycall_ffi_mock_call_count() == 1);
    assert(polycall_ffi_mock_last_run() == 1);
    assert(std::string(polycall_ffi_mock_last_config()) == config_path);

    polycall_ffi_mock_return_status(37);
    assert(polycall::run_config(config_path) == 37);

    try {
        polycall::run_config_or_throw(config_path);
        assert(false && "run_config_or_throw must throw on failure");
    } catch (const polycall::Error& error) {
        assert(error.code() == 37);
        assert(error.status() == 37);
    }

    std::cout << "cpp-polycall adapter test: PASS\n";
    return 0;
}
