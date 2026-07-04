#include "cpp_polycall/polycall.hpp"

#include <polycall/polycall_ffi.h>

#include <string>

namespace polycall {

int run_config(const std::string& config_path) {
    return polycall_ffi_run_config(config_path.c_str(), 1);
}

void run_config_or_throw(const std::string& config_path) {
    const int status = run_config(config_path);
    if (status != 0) {
        throw Error(
            "libpolycall failed for '" + config_path +
                "' (status=" + std::to_string(status) + ")",
            status);
    }
}

} // namespace polycall
