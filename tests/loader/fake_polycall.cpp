// TEST FIXTURE -- NOT THE POLYCALL CORE.
//
// Stand-ins for the shared library (libpolycall.so.1 / polycall.dll /
// libpolycall.dll) used ONLY by tests/loader-errors.sh to prove that
// cpp-polycall refuses a library that does not implement binding ABI v1:
//
//   FAKE_ABI2  every ABI v1 symbol exists, but polycall_ffi_abi_version()
//              reports 2; every other entry point returns POLYCALL_E_INTERNAL
//              so a call that slipped past the ABI check is visible.
//   FAKE_OLD   a 1.0-era library: only the 1.0 symbols, no ABI v1 surface.
//
// No functional check of the binding uses these; they run against the real
// installed core (tests/real_core_test.cpp).
#include <cstddef>
#include <cstdint>

#if defined(_WIN32)
#define FAKE_API extern "C" __declspec(dllexport)
#else
#define FAKE_API extern "C" __attribute__((visibility("default")))
#endif

namespace {
constexpr int kInternal = -18; // POLYCALL_E_INTERNAL
}

FAKE_API const char* polycall_get_version(void) { return "1.0.0-fake"; }
FAKE_API void* polycall_get_last_error(void* ctx) { (void)ctx; return nullptr; }

#if defined(FAKE_ABI2)
FAKE_API int polycall_ffi_abi_version(void) { return 2; }
FAKE_API int polycall_ffi_version(char* buf, int len) {
    static const char v[] = "2.0.0-fake";
    const int n = static_cast<int>(sizeof v) - 1;
    if (len < 0) return -1;
    if (len > 0) {
        int i = 0;
        for (; i < n && i < len - 1; ++i) buf[i] = v[i];
        buf[i] = '\0';
    }
    return n;
}
FAKE_API const char* polycall_strerror(int status) { (void)status; return "POLYCALL_E_INTERNAL: fake library"; }
FAKE_API int polycall_last_error(char* buf, std::size_t cap) { if (buf && cap) buf[0] = '\0'; return 0; }
FAKE_API int polycall_ffi_run_config(const char*, int) { return kInternal; }
FAKE_API int polycall_ffi_describe(const char*, char*, int) { return kInternal; }
FAKE_API int polycall_call(const char*, const char*, const char*, const char*, std::uint32_t, char*,
                           std::size_t, std::size_t*) { return kInternal; }
FAKE_API int polycall_peer_open(const char*, const char*, const char*, std::int32_t*) { return kInternal; }
FAKE_API int polycall_peer_close(std::int32_t) { return kInternal; }
FAKE_API int polycall_peer_endpoint(std::int32_t, char*, std::size_t) { return kInternal; }
FAKE_API int polycall_peer_node_id(std::int32_t, char*, std::size_t) { return kInternal; }
FAKE_API int polycall_peer_register(std::int32_t, const char*, const char*) { return kInternal; }
FAKE_API int polycall_peer_unregister(std::int32_t, const char*) { return kInternal; }
FAKE_API int polycall_peer_list(std::int32_t, char*, std::size_t, std::size_t*) { return kInternal; }
FAKE_API int polycall_peer_ping(std::int32_t, const char*, std::uint32_t) { return kInternal; }
FAKE_API int polycall_peer_send(std::int32_t, const char*, const void*, std::size_t, const char*,
                                std::uint32_t) { return kInternal; }
FAKE_API int polycall_peer_recv(std::int32_t, std::uint32_t, char*, std::size_t, char*, std::size_t, void*,
                                std::size_t, std::size_t*) { return kInternal; }
FAKE_API int polycall_peer_cancel(std::int32_t) { return kInternal; }
FAKE_API int polycall_peer_health(std::int32_t, char*, std::size_t, std::size_t*) { return kInternal; }
#elif !defined(FAKE_OLD)
#error "define FAKE_ABI2 or FAKE_OLD"
#endif
