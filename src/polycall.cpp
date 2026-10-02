#include "cpp_polycall/polycall.hpp"

#include <polycall.h>

#include <algorithm>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace polycall {

namespace {

std::string capture_last_error() {
    char buf[2048];
    buf[0] = '\0';
    const int n = polycall_last_error(buf, sizeof buf);
    if (n < 0) {
        return std::string();
    }
    return std::string(buf);
}

std::string compose(int st, const std::string& name, const std::string& detail,
                    const std::string& context) {
    std::string msg;
    if (!context.empty()) {
        msg += context;
        msg += ": ";
    }
    msg += name;
    if (!detail.empty()) {
        msg += " (";
        msg += detail;
        msg += ")";
    }
    msg += " [status=" + std::to_string(st) + "]";
    return msg;
}

[[noreturn]] void raise(int st, const std::string& context,
                        std::string output = std::string()) {
    // polycall_last_error is per thread: read it before anything else runs.
    std::string detail = capture_last_error();
    throw Error(st, polycall_strerror(st), std::move(detail), context,
                std::move(output));
}

void check(int st, const char* context) {
    if (st != POLYCALL_OK) {
        raise(st, context);
    }
}

// ABI 1 is verified once per process before the first ABI call.
int abi_status() noexcept {
    static std::once_flag once;
    static int verdict = POLYCALL_OK;
    std::call_once(once, [] {
        verdict = polycall_ffi_abi_version() == expected_abi_version
                      ? POLYCALL_OK
                      : POLYCALL_E_UNSUPPORTED;
    });
    return verdict;
}

void require_abi() {
    if (abi_status() != POLYCALL_OK) {
        check_abi();
    }
}

std::string read_text(int (*fn)(polycall_peer_t, char*, std::size_t),
                      polycall_peer_t h, const char* context) {
    std::vector<char> buf(256);
    const int st = fn(h, buf.data(), buf.size());
    check(st, context);
    return std::string(buf.data());
}

std::string read_json(int (*fn)(polycall_peer_t, char*, std::size_t, std::size_t*),
                      polycall_peer_t h, const char* context) {
    std::vector<char> buf(4096);
    for (int attempt = 0; attempt < 4; ++attempt) {
        std::size_t needed = 0;
        const int st = fn(h, buf.data(), buf.size(), &needed);
        if (st == POLYCALL_E_TOO_LARGE && needed >= buf.size()) {
            buf.resize(needed + 1);
            continue;
        }
        check(st, context);
        return std::string(buf.data(), needed);
    }
    raise(POLYCALL_E_TOO_LARGE, context);
}

} // namespace

Error::Error(int st, std::string name, std::string detail, std::string context,
             std::string output)
    : std::runtime_error(compose(st, name, detail, context)),
      status_(st),
      name_(std::move(name)),
      detail_(std::move(detail)),
      output_(std::move(output)) {}

// ---- library ---------------------------------------------------------------

int abi_version() noexcept {
    return polycall_ffi_abi_version();
}

std::string version() {
    char buf[64];
    const int n = polycall_ffi_version(buf, static_cast<int>(sizeof buf));
    if (n < 0) {
        raise(n, "polycall_ffi_version");
    }
    return std::string(buf);
}

std::string strerror(int st) {
    return std::string(polycall_strerror(st));
}

std::string last_error() {
    return capture_last_error();
}

void check_abi() {
    const int got = polycall_ffi_abi_version();
    if (got != expected_abi_version) {
        throw Error(POLYCALL_E_UNSUPPORTED, polycall_strerror(POLYCALL_E_UNSUPPORTED),
                    "libpolycall reports binding ABI " + std::to_string(got) +
                        ", cpp-polycall requires ABI " +
                        std::to_string(expected_abi_version),
                    "check_abi");
    }
}

// ---- configuration -----------------------------------------------------------

int run_config(const std::string& config_path) {
    return run_config(config_path, true);
}

int run_config(const std::string& config_path, bool strict) {
    if (abi_status() != POLYCALL_OK) {
        return POLYCALL_E_UNSUPPORTED;
    }
    return polycall_ffi_run_config(config_path.c_str(), strict ? 1 : 0);
}

void run_config_or_throw(const std::string& config_path, bool strict) {
    require_abi();
    const int st = polycall_ffi_run_config(config_path.c_str(), strict ? 1 : 0);
    if (st != POLYCALL_OK) {
        raise(st, "run_config('" + config_path + "')");
    }
}

std::string describe(const std::string& config_path) {
    require_abi();
    std::vector<char> buf(16 * 1024);
    for (int attempt = 0; attempt < 4; ++attempt) {
        const int n = polycall_ffi_describe(config_path.c_str(), buf.data(),
                                            static_cast<int>(buf.size()));
        if (n < 0) {
            raise(n, "describe('" + config_path + "')");
        }
        if (static_cast<std::size_t>(n) < buf.size()) {
            return std::string(buf.data(), static_cast<std::size_t>(n));
        }
        buf.resize(static_cast<std::size_t>(n) + 1);
    }
    raise(POLYCALL_E_TOO_LARGE, "describe('" + config_path + "')");
}

// ---- RPC ---------------------------------------------------------------------

std::string call(const std::string& endpoint, const std::string& service,
                 const std::string& operation, const std::string& input_json,
                 std::uint32_t timeout_ms) {
    require_abi();
    // A too-small buffer would discard a result that already ran: size it for
    // the documented maximum.
    std::vector<char> out(POLYCALL_CALL_MAX_OUTPUT + 1);
    std::size_t len = 0;
    const int st = polycall_call(endpoint.c_str(), service.c_str(), operation.c_str(),
                                 input_json.empty() ? nullptr : input_json.c_str(),
                                 timeout_ms, out.data(), out.size(), &len);
    if (st != POLYCALL_OK) {
        std::string remote;
        if (st == POLYCALL_E_REMOTE || st == POLYCALL_E_NOT_FOUND ||
            st == POLYCALL_E_TIMEOUT || st == POLYCALL_E_BUSY) {
            out.back() = '\0';
            remote = std::string(out.data()); // NUL-terminated error object
        }
        raise(st, "call(" + service + "." + operation + " @ " + endpoint + ")",
              std::move(remote));
    }
    return std::string(out.data(), len);
}

// ---- peer nodes --------------------------------------------------------------

Peer::Peer(const std::string& node_id, const std::string& bind_endpoint,
           const std::string& auth_token) {
    open(node_id, bind_endpoint.c_str(), auth_token);
}

void Peer::open(const std::string& node_id, const char* bind_endpoint,
                const std::string& auth_token) {
    require_abi();
    polycall_peer_t h = 0;
    const int st = polycall_peer_open(node_id.c_str(), bind_endpoint,
                                      auth_token.empty() ? nullptr : auth_token.c_str(),
                                      &h);
    if (st != POLYCALL_OK) {
        raise(st, "Peer('" + node_id + "')");
    }
    handle_ = h;
    open_ = true;
}

Peer Peer::send_only(const std::string& node_id, const std::string& auth_token) {
    Peer p;
    p.open(node_id, nullptr, auth_token);
    return p;
}

Peer::Peer(Peer&& other) noexcept : handle_(other.handle_), open_(other.open_) {
    other.handle_ = 0;
    other.open_ = false;
}

Peer& Peer::operator=(Peer&& other) noexcept {
    if (this != &other) {
        release();
        handle_ = other.handle_;
        open_ = other.open_;
        other.handle_ = 0;
        other.open_ = false;
    }
    return *this;
}

Peer::~Peer() {
    release();
}

void Peer::release() noexcept {
    if (open_) {
        (void)polycall_peer_close(handle_);
        open_ = false;
    }
}

void Peer::close() {
    // Always ask the core: a second close is POLYCALL_E_INVALID_HANDLE.
    const int st = polycall_peer_close(handle_);
    open_ = false;
    check(st, "Peer::close");
}

std::string Peer::endpoint() const {
    return read_text(polycall_peer_endpoint, handle_, "Peer::endpoint");
}

std::string Peer::node_id() const {
    return read_text(polycall_peer_node_id, handle_, "Peer::node_id");
}

void Peer::register_peer(const std::string& peer_id, const std::string& endpoint) {
    check(polycall_peer_register(handle_, peer_id.c_str(), endpoint.c_str()),
          "Peer::register_peer");
}

void Peer::unregister_peer(const std::string& peer_id) {
    check(polycall_peer_unregister(handle_, peer_id.c_str()), "Peer::unregister_peer");
}

std::string Peer::list() const {
    return read_json(polycall_peer_list, handle_, "Peer::list");
}

void Peer::ping(const std::string& peer, std::uint32_t timeout_ms) {
    check(polycall_peer_ping(handle_, peer.c_str(), timeout_ms), "Peer::ping");
}

void Peer::send(const std::string& peer, const void* data, std::size_t len,
                const std::string& message_id, std::uint32_t timeout_ms) {
    static const unsigned char empty = 0;
    const int st = polycall_peer_send(handle_, peer.c_str(), data ? data : &empty, len,
                                      message_id.empty() ? nullptr : message_id.c_str(),
                                      timeout_ms);
    check(st, "Peer::send");
}

void Peer::send(const std::string& peer, std::string_view payload,
                const std::string& message_id, std::uint32_t timeout_ms) {
    send(peer, payload.data(), payload.size(), message_id, timeout_ms);
}

void Peer::send(const std::string& peer, const std::vector<std::uint8_t>& payload,
                const std::string& message_id, std::uint32_t timeout_ms) {
    send(peer, payload.data(), payload.size(), message_id, timeout_ms);
}

bool Peer::try_recv(Message& out, std::uint32_t timeout_ms, std::size_t initial_capacity) {
    std::vector<std::uint8_t> buf(std::max<std::size_t>(initial_capacity, 1));
    char sender[POLYCALL_PEER_ID_MAX];
    char mid[POLYCALL_MESSAGE_ID_MAX];
    for (;;) {
        std::size_t len = 0;
        const int st = polycall_peer_recv(handle_, timeout_ms, sender, sizeof sender, mid,
                                          sizeof mid, buf.data(), buf.size(), &len);
        if (st == POLYCALL_E_TOO_LARGE && len > buf.size()) {
            buf.resize(len); // message stays queued: retry with room for it
            continue;
        }
        if (st == POLYCALL_E_TIMEOUT) {
            return false;
        }
        check(st, "Peer::recv");
        out.sender = sender;
        out.id = mid;
        out.payload.assign(buf.begin(), buf.begin() + static_cast<std::ptrdiff_t>(len));
        return true;
    }
}

Message Peer::recv(std::uint32_t timeout_ms, std::size_t initial_capacity) {
    Message m;
    if (!try_recv(m, timeout_ms, initial_capacity)) {
        raise(POLYCALL_E_TIMEOUT, "Peer::recv");
    }
    return m;
}

void Peer::cancel() {
    check(polycall_peer_cancel(handle_), "Peer::cancel");
}

std::string Peer::health() const {
    return read_json(polycall_peer_health, handle_, "Peer::health");
}

} // namespace polycall
