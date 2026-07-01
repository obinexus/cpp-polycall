/*
 * cpp-polycall - C++ reference adapter for libpolycall (header-only, RAII).
 *
 * A thin idiomatic wrapper over the C ABI. No runtime/config logic lives here:
 * every method forwards across the FFI boundary. RAII guarantees the context is
 * always freed. This is the template for other native OOP bindings.
 */
#ifndef POLYCALL_HPP
#define POLYCALL_HPP

#include "polycall/polycall.h"

#include <stdexcept>
#include <string>

namespace polycall {

inline std::string version() { return polycall_version(); }

class Error : public std::runtime_error {
public:
    explicit Error(const std::string& what, int code)
        : std::runtime_error(what), code_(code) {}
    int code() const noexcept { return code_; }
private:
    int code_;
};

class Context {
public:
    Context() {
        int rc = polycall_init(&ctx_);
        if (rc != POLYCALL_OK)
            throw Error(std::string("init: ") + polycall_status_str(rc), rc);
    }
    ~Context() { polycall_free(ctx_); }

    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&& o) noexcept : ctx_(o.ctx_) { o.ctx_ = nullptr; }

    void load(const std::string& path) { check(polycall_load_config(ctx_, path.c_str())); }
    void verify()                      { check(polycall_verify(ctx_)); }
    void run()                         { check(polycall_run(ctx_)); }
    void inspect()                     { check(polycall_inspect(ctx_)); }

    std::string last_error() const { return polycall_last_error(ctx_); }

private:
    void check(int rc) {
        if (rc != POLYCALL_OK)
            throw Error(last_error().empty() ? polycall_status_str(rc) : last_error(), rc);
    }
    polycall_context_t* ctx_ = nullptr;
};

} // namespace polycall

#endif // POLYCALL_HPP
