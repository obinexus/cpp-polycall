/*
 * cpp-polycall example.
 * Build (after core is built):
 *   g++ -std=c++17 -I../../include -I../src main.cpp ../../build/libpolycall.a -o cpp-polycall-demo
 * Run:
 *   ./cpp-polycall-demo ../cpp-polycallrc
 */
#include "polycall.hpp"

#include <iostream>

int main(int argc, char** argv) {
    const std::string cfg = (argc > 1) ? argv[1] : "cpp-polycallrc";
    try {
        std::cout << "cpp-polycall using libpolycall " << polycall::version() << "\n";
        polycall::Context ctx;
        ctx.load(cfg);
        ctx.inspect();
        ctx.run();
        return 0;
    } catch (const polycall::Error& e) {
        std::cerr << "cpp-polycall error(" << e.code() << "): " << e.what() << "\n";
        return e.code();
    }
}
