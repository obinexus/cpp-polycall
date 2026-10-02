#include "cpp_polycall/polycall.hpp"

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string config_path =
        argc > 1 ? argv[1] : polycall::default_config;

    try {
        polycall::check_abi();
        std::cout << "cpp-polycall: libpolycall " << polycall::version()
                  << " (binding ABI " << polycall::abi_version() << ")\n";
        polycall::run_config_or_throw(config_path);
        std::cout << "cpp-polycall: '" << config_path << "' is valid for this build\n";

        // Two nodes in this process, exchanging one payload.
        polycall::Peer alpha("alpha");
        polycall::Peer beta("beta");
        alpha.register_peer("beta", beta.endpoint());
        alpha.send("beta", std::string_view("hello from C++"), "example-1");
        const polycall::Message m = beta.recv(5000);
        std::cout << "beta received '" << m.text() << "' from " << m.sender
                  << " (id " << m.id << ")\n";
        return 0;
    } catch (const polycall::Error& error) {
        std::cerr << "cpp-polycall: " << error.what() << '\n';
        return 1;
    }
}
