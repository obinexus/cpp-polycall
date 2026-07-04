#include "cpp_polycall/polycall.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string config_path =
        argc > 1 ? argv[1] : polycall::default_config;

    try {
        polycall::run_config_or_throw(config_path);
        std::cout << "cpp-polycall: configuration started successfully\n";
        return 0;
    } catch (const polycall::Error& error) {
        std::cerr << "cpp-polycall: " << error.what() << '\n';
        return error.code();
    }
}
