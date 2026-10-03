// Consumer of an installed cpp-polycall (find_package packaging check).
#include <cpp_polycall/polycall.hpp>

#include <iostream>
#include <string>

int main() {
    try {
        polycall::check_abi();
        polycall::Peer alpha("consumer-a");
        polycall::Peer beta("consumer-b");
        alpha.send(beta.endpoint(), std::string_view("installed package"), "consumer-1");
        const polycall::Message m = beta.recv(5000);
        const bool ok = m.sender == "consumer-a" && m.id == "consumer-1" && m.text() == "installed package" &&
                        polycall::run_config("", true) == polycall::status::invalid_argument;
        std::cout << (ok ? "PASS" : "FAIL") << " installed cpp-polycall " << polycall::version()
                  << " (ABI " << polycall::abi_version() << "): peer round trip, run_config\n";
        return ok ? 0 : 1;
    } catch (const polycall::Error& e) {
        std::cout << "FAIL " << e.what() << '\n';
        return 1;
    }
}
