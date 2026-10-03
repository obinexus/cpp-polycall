// cpp-polycall loader / ABI guard program, driven by tests/loader-errors.sh.
//
//   abi_guard_test expect-ok        the loaded library speaks binding ABI 1
//   abi_guard_test expect-mismatch  the loaded library reports another ABI:
//                                   every entry point must refuse with
//                                   polycall::Error(status::unsupported)
//                                   before reaching the library.
// Prints PASS/FAIL lines; exit 0 only when every check passed.
#include "cpp_polycall/polycall.hpp"

#include <cstdio>
#include <string>

namespace {

int g_fail = 0;

void check(bool ok, const std::string& name, const std::string& detail = std::string()) {
    std::printf("%s %s%s%s\n", ok ? "PASS" : "FAIL", name.c_str(), detail.empty() ? "" : " -- ",
                detail.c_str());
    if (!ok) {
        ++g_fail;
    }
}

template <class F>
int status_of(F&& f, std::string* what) {
    try {
        f();
        return 0;
    } catch (const polycall::Error& e) {
        *what = e.what();
        return e.status();
    } catch (const std::exception& e) {
        *what = std::string("non-polycall exception: ") + e.what();
        return 12345;
    }
}

} // namespace

int main(int argc, char** argv) {
    const std::string mode = argc > 1 ? argv[1] : "expect-ok";
    std::string what;
    std::printf("abi_guard: library reports binding ABI %d\n", polycall::abi_version());
    if (mode == "expect-ok") {
        check(status_of([] { polycall::check_abi(); }, &what) == 0, "real library: check_abi() accepts ABI 1", what);
        check(polycall::run_config("", true) == polycall::status::invalid_argument,
              "real library: run_config reaches the core (empty path -> E_INVALID_ARGUMENT)");
    } else if (mode == "expect-mismatch") {
        const int st = status_of([] { polycall::check_abi(); }, &what);
        check(st == polycall::status::unsupported && what.find("ABI 2") != std::string::npos &&
                  what.find("requires ABI 1") != std::string::npos,
              "ABI 2 library: check_abi() throws polycall::Error(E_UNSUPPORTED) naming both ABIs", what);
        check(polycall::run_config("cpp-polycallrc") == polycall::status::unsupported,
              "ABI 2 library: run_config refuses with E_UNSUPPORTED (core not called)");
        check(status_of([] { polycall::run_config_or_throw("cpp-polycallrc"); }, &what) ==
                  polycall::status::unsupported,
              "ABI 2 library: run_config_or_throw refuses", what);
        check(status_of([] { polycall::describe("cpp-polycallrc"); }, &what) == polycall::status::unsupported,
              "ABI 2 library: describe refuses", what);
        check(status_of([] { polycall::call("127.0.0.1:1", "debug", "echo", "{}", 1000); }, &what) ==
                  polycall::status::unsupported,
              "ABI 2 library: call refuses", what);
        check(status_of([] { polycall::Peer p("guard"); }, &what) == polycall::status::unsupported,
              "ABI 2 library: Peer refuses to open", what);
    } else {
        std::fprintf(stderr, "usage: abi_guard_test expect-ok|expect-mismatch\n");
        return 2;
    }
    return g_fail == 0 ? 0 : 1;
}
