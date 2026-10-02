#ifndef CPP_POLYCALL_POLYCALL_HPP
#define CPP_POLYCALL_POLYCALL_HPP

// cpp-polycall: C++17 RAII wrapper over the Polycall binding ABI v1
// (<polycall.h>, docs/BINDING_ABI.md in https://github.com/obinexus/polycall).
//
// The wrapper links against the installed core (CMake package
// polycall::polycall or `pkg-config polycall`). It owns no protocol, parser or
// runtime logic: every operation is one call into libpolycall.

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace polycall {

constexpr const char default_config[] = "cpp-polycallrc";

// The binding ABI this wrapper was written for (POLYCALL_FFI_ABI_VERSION).
constexpr int expected_abi_version = 1;

// Status codes of the binding ABI (mirrors POLYCALL_OK / POLYCALL_E_*).
namespace status {
constexpr int ok = 0;
constexpr int invalid_argument = -1;
constexpr int no_memory = -2;
constexpr int invalid_handle = -3;
constexpr int timeout = -4;
constexpr int transport = -5;
constexpr int protocol = -6;
constexpr int not_found = -7;
constexpr int auth = -8;
constexpr int remote = -9;
constexpr int too_large = -10;
constexpr int busy = -11;
constexpr int cancelled = -12;
constexpr int config = -13;
constexpr int address_in_use = -14;
constexpr int unsupported = -15;
constexpr int permission = -16;
constexpr int closed = -17;
constexpr int internal = -18;
} // namespace status

// Limits of the binding ABI.
constexpr std::size_t max_payload = std::size_t(1) << 20;     // POLYCALL_PEER_MAX_PAYLOAD
constexpr std::size_t max_call_output = std::size_t(1) << 20; // POLYCALL_CALL_MAX_OUTPUT
constexpr std::uint32_t wait_forever = 0xFFFFFFFFu;           // recv: block until message/cancel/close

// A failed libpolycall call. Carries the status code, its name
// (polycall_strerror) and the calling thread's detail (polycall_last_error).
class Error final : public std::runtime_error {
public:
    Error(int status, std::string name, std::string detail,
          std::string context = std::string(), std::string output = std::string());

    int status() const noexcept { return status_; }
    int code() const noexcept { return status_; }
    // e.g. "POLYCALL_E_TIMEOUT: deadline exceeded"
    const std::string& name() const noexcept { return name_; }
    // polycall_last_error() captured right after the failing call
    const std::string& detail() const noexcept { return detail_; }
    // call(): the remote error object {"code":..,"message":..} when present
    const std::string& output() const noexcept { return output_; }

private:
    int status_;
    std::string name_;
    std::string detail_;
    std::string output_;
};

// ---- library ---------------------------------------------------------------

int abi_version() noexcept;              // polycall_ffi_abi_version()
std::string version();                   // polycall_ffi_version(), e.g. "1.1.0"
std::string strerror(int status);        // polycall_strerror()
std::string last_error();                // polycall_last_error() of this thread
// Throws Error(status::unsupported) unless the loaded library speaks ABI 1.
void check_abi();

// ---- configuration -----------------------------------------------------------

// Historical entry point: polycall_ffi_run_config(config_path, 1). Returns the
// core status unchanged (0 on success).
int run_config(const std::string& config_path = default_config);
// strict=false validates (unknown keys are warnings); strict=true validates
// for running with this build (unknown keys and tls_enabled=true rejected).
int run_config(const std::string& config_path, bool strict);
// As run_config, but throws Error on a nonzero status.
void run_config_or_throw(const std::string& config_path = default_config,
                         bool strict = true);
// polycall_ffi_describe(): JSON description of the configuration file.
std::string describe(const std::string& config_path);

// ---- RPC ---------------------------------------------------------------------

// One polycall_rpc v1 round trip (never retried). Returns the operation's
// output JSON; throws Error (with output() = remote error object) otherwise.
// input_json empty -> JSON null. timeout_ms must be 1..600000.
std::string call(const std::string& endpoint, const std::string& service,
                 const std::string& operation, const std::string& input_json,
                 std::uint32_t timeout_ms);

// ---- peer nodes --------------------------------------------------------------

struct Message {
    std::string sender;
    std::string id;
    std::vector<std::uint8_t> payload;

    std::string text() const { return std::string(payload.begin(), payload.end()); }
};

// One NSIGII-style peer node (polycall_peer_*). Move-only; the destructor
// closes the node. close() is explicit and reports the core's verdict: a
// second close() throws Error(status::invalid_handle), as does any call on a
// closed node.
class Peer {
public:
    // Listening node; bind_endpoint "127.0.0.1:0" = ephemeral loopback port.
    explicit Peer(const std::string& node_id,
                  const std::string& bind_endpoint = "127.0.0.1:0",
                  const std::string& auth_token = std::string());
    // Node without a listener (can send, ping and keep a registry).
    static Peer send_only(const std::string& node_id,
                          const std::string& auth_token = std::string());

    Peer(Peer&& other) noexcept;
    Peer& operator=(Peer&& other) noexcept;
    Peer(const Peer&) = delete;
    Peer& operator=(const Peer&) = delete;
    ~Peer();

    void close();
    bool is_open() const noexcept { return open_; }
    std::int32_t handle() const noexcept { return handle_; }

    std::string endpoint() const;            // "host:port" ("" when send-only)
    std::string node_id() const;
    void register_peer(const std::string& peer_id, const std::string& endpoint);
    void unregister_peer(const std::string& peer_id);
    std::string list() const;                // {"id":"host:port",...}
    void ping(const std::string& peer, std::uint32_t timeout_ms = 3000);
    // Exactly one delivery attempt; message_id empty -> generated by the core.
    void send(const std::string& peer, const void* data, std::size_t len,
              const std::string& message_id = std::string(),
              std::uint32_t timeout_ms = 5000);
    void send(const std::string& peer, std::string_view payload,
              const std::string& message_id = std::string(),
              std::uint32_t timeout_ms = 5000);
    void send(const std::string& peer, const std::vector<std::uint8_t>& payload,
              const std::string& message_id = std::string(),
              std::uint32_t timeout_ms = 5000);
    // Oldest message; throws Error(status::timeout) when none arrived.
    // A buffer smaller than the message is grown and the call retried (the
    // core keeps the message queued on POLYCALL_E_TOO_LARGE).
    Message recv(std::uint32_t timeout_ms = wait_forever,
                 std::size_t initial_capacity = 64 * 1024);
    // As recv(), but returns false instead of throwing on timeout.
    bool try_recv(Message& out, std::uint32_t timeout_ms = 0,
                  std::size_t initial_capacity = 64 * 1024);
    void cancel();                           // wake blocked recv() (cancelled)
    std::string health() const;              // JSON

private:
    Peer() = default;
    void open(const std::string& node_id, const char* bind_endpoint,
              const std::string& auth_token);
    void release() noexcept;

    std::int32_t handle_ = 0;
    bool open_ = false;
};

} // namespace polycall

#endif // CPP_POLYCALL_POLYCALL_HPP
