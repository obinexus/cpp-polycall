#ifndef CPP_POLYCALL_POLYCALL_HPP
#define CPP_POLYCALL_POLYCALL_HPP

#include <stdexcept>
#include <string>
#include <utility>

namespace polycall {

constexpr const char default_config[] = "cpp-polycallrc";

class Error final : public std::runtime_error {
public:
    Error(std::string message, int status)
        : std::runtime_error(std::move(message)), status_(status) {}

    int status() const noexcept { return status_; }
    int code() const noexcept { return status_; }

private:
    int status_;
};

// Return the libpolycall status unchanged.
int run_config(const std::string& config_path = default_config);

// Raise Error when libpolycall returns a nonzero status.
void run_config_or_throw(const std::string& config_path = default_config);

} // namespace polycall

#endif // CPP_POLYCALL_POLYCALL_HPP
