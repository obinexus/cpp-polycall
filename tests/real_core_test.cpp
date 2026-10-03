// cpp-polycall tests against the REAL installed libpolycall (no mock).
//
//   real_core_test <repo-root>
//
// Covers the checklist of docs/BINDING_ABI.md. Checks that need the C CLI
// (cross-language interop, polycall_call against `polycall start`) read
//   POLYCALL_CLI           path of the polycall program
//   POLYCALL_CLI_PEER      host:port of a running `polycall peer serve` node
//   POLYCALL_RPC_ENDPOINT  host:port of a running `polycall start` runtime
//   POLYCALL_DAEMON_ENDPOINT host:port of a running `polycall daemon start`
//   POLYCALL_DEV_TOKEN     shared token of those nodes
// (tests/run-real.sh starts them). Without them those checks print SKIP --
// they are never counted as passed, and the program exits 77 (CTest SKIP)
// when any check was skipped and none failed. Every check works with
// NDEBUG: no assert().
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "cpp_polycall/polycall.hpp"

#include <polycall.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

int g_pass = 0;
int g_fail = 0;
int g_skip = 0;

void check(bool ok, const std::string& name, const std::string& detail = std::string()) {
    if (ok) {
        ++g_pass;
        std::printf("PASS %s\n", name.c_str());
    } else {
        ++g_fail;
        std::printf("FAIL %s%s%s\n", name.c_str(), detail.empty() ? "" : " -- ",
                    detail.c_str());
    }
    std::fflush(stdout);
}

void skip(const std::string& name, const std::string& reason) {
    ++g_skip;
    std::printf("SKIP %s -- %s\n", name.c_str(), reason.c_str());
    std::fflush(stdout);
}

// Status a callable throws as polycall::Error (0 when it returns normally).
template <class F>
int status_of(F&& f, std::string* what = nullptr) {
    try {
        f();
        return 0;
    } catch (const polycall::Error& e) {
        if (what) {
            *what = e.what();
        }
        return e.status();
    } catch (const std::exception& e) {
        if (what) {
            *what = std::string("non-polycall exception: ") + e.what();
        }
        return 12345;
    }
}

void expect_status(int expected, int got, const std::string& name, const std::string& what = "") {
    check(got == expected, name,
          "expected " + std::to_string(expected) + " got " + std::to_string(got) +
              (what.empty() ? "" : " (" + what + ")"));
}

std::string env(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

bool contains(const std::string& hay, const std::string& needle) {
    return hay.find(needle) != std::string::npos;
}

std::vector<std::uint8_t> bytes(const std::string& s) {
    return std::vector<std::uint8_t>(s.begin(), s.end());
}

std::string base64(const std::vector<std::uint8_t>& in) {
    static const char tbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    std::size_t i = 0;
    for (; i + 2 < in.size(); i += 3) {
        const unsigned v = (unsigned(in[i]) << 16) | (unsigned(in[i + 1]) << 8) | in[i + 2];
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += tbl[v & 63];
    }
    if (i + 1 == in.size()) {
        const unsigned v = unsigned(in[i]) << 16;
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += "==";
    } else if (i + 2 == in.size()) {
        const unsigned v = (unsigned(in[i]) << 16) | (unsigned(in[i + 1]) << 8);
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += '=';
    }
    return out;
}

int run_command(const std::string& cmdline) {
#if defined(_WIN32)
    // cmd.exe /c strips one pair of outer quotes: add them.
    const std::string c = "\"" + cmdline + "\"";
#else
    const std::string c = cmdline;
#endif
    return std::system(c.c_str());
}

std::string slurp(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

// ---------------------------------------------------------------------------

void test_version_and_abi() {
    check(polycall::abi_version() == 1, "abi: polycall_ffi_abi_version() == 1");
    check(status_of([] { polycall::check_abi(); }) == 0, "abi: check_abi() accepts the library");
    const std::string v = polycall::version();
    check(v.rfind("1.", 0) == 0 && v >= "1.1.0", "version: library >= 1.1.0", v);
    char small[2];
    check(polycall_ffi_version(small, -1) == POLYCALL_E_INVALID_ARGUMENT,
          "version: negative length -> E_INVALID_ARGUMENT");
    check(polycall_ffi_version(small, 2) == static_cast<int>(v.size()) && small[1] == '\0',
          "version: snprintf rules on a 2-byte buffer");
    check(polycall::strerror(polycall::status::timeout).rfind("POLYCALL_E_TIMEOUT", 0) == 0,
          "strerror names E_TIMEOUT");
    check(polycall::strerror(-999).rfind("POLYCALL_E_UNKNOWN", 0) == 0,
          "strerror of an unknown code");
}

void test_run_config(const std::string& root) {
    const std::string valid = root + "/cpp-polycallrc";
    const std::string fx = root + "/tests/fixtures/";
    expect_status(0, polycall::run_config(valid), "run_config: shipped cpp-polycallrc (run=1)");
    expect_status(0, polycall::run_config(valid, false), "run_config: valid, validate-only");
    expect_status(0, polycall::run_config(root + "/examples/cpp-polycallrc"),
                  "run_config: examples/cpp-polycallrc");

    std::string what;
    const int missing = status_of([&] { polycall::run_config_or_throw(fx + "does-not-exist"); });
    expect_status(polycall::status::not_found, missing, "run_config: missing file -> E_NOT_FOUND");
    try {
        polycall::run_config_or_throw(fx + "does-not-exist");
    } catch (const polycall::Error& e) {
        check(e.name().rfind("POLYCALL_E_NOT_FOUND", 0) == 0 && contains(e.detail(), "does-not-exist") &&
                  contains(e.what(), "does-not-exist"),
              "Error carries status name + polycall_last_error detail", e.what());
    }
    expect_status(polycall::status::config, polycall::run_config(fx + "invalid-polycallrc", false),
                  "run_config: malformed file -> E_CONFIG (validate)");
    expect_status(polycall::status::config, polycall::run_config(fx + "invalid-polycallrc", true),
                  "run_config: malformed file -> E_CONFIG (strict)");
    expect_status(0, polycall::run_config(fx + "unknown-key-polycallrc", false),
                  "run_config: unknown key is a warning when validating");
    expect_status(polycall::status::config, polycall::run_config(fx + "unknown-key-polycallrc", true),
                  "run_config: unknown key is an error when strict");
    expect_status(0, polycall::run_config(fx + "tls-polycallrc", false),
                  "run_config: tls_enabled=true validates");
    expect_status(polycall::status::unsupported, polycall::run_config(fx + "tls-polycallrc", true),
                  "run_config: tls_enabled=true strict -> E_UNSUPPORTED");
    expect_status(polycall::status::invalid_argument, polycall::run_config("", true),
                  "run_config: empty path -> E_INVALID_ARGUMENT");
    const std::string desc = polycall::describe(valid);
    check(!desc.empty() && desc.front() == '{' && contains(desc, "log_level"),
          "describe: JSON with the file's keys", desc.substr(0, 120));
}

void test_call() {
    std::string what;
    expect_status(polycall::status::transport,
                  status_of([] { polycall::call("127.0.0.1:1", "debug", "echo", "{}", 1500); }, &what),
                  "call: no runtime -> E_TRANSPORT", what);
    expect_status(polycall::status::invalid_argument,
                  status_of([] { polycall::call("127.0.0.1:1", "debug", "echo", "{}", 0); }),
                  "call: timeout 0 -> E_INVALID_ARGUMENT");
    const std::string ep = env("POLYCALL_RPC_ENDPOINT");
    if (ep.empty()) {
        for (const char* n : {"call: debug.echo", "call: inventory.get", "call: unknown operation",
                              "call: unknown item", "call: deadline", "call: invalid input JSON"}) {
            skip(n, "POLYCALL_RPC_ENDPOINT not set (run via tests/run-real.sh)");
        }
        return;
    }
    std::string out = polycall::call(ep, "debug", "echo", "{\"x\":1,\"s\":\"h\xc3\xa9\"}", 3000);
    check(out == "{\"echo\":{\"x\":1,\"s\":\"h\xc3\xa9\"}}", "call: debug.echo exact output", out);
    out = polycall::call(ep, "inventory", "get", "{\"item_id\":\"widget-a\"}", 3000);
    check(out == "{\"item_id\":\"widget-a\",\"quantity\":42,\"in_stock\":true}",
          "call: inventory.get exact output", out);
    try {
        polycall::call(ep, "inventory", "teleport", "{}", 3000);
        check(false, "call: unknown operation -> E_NOT_FOUND", "no error");
    } catch (const polycall::Error& e) {
        check(e.status() == polycall::status::not_found && contains(e.output(), "operation.unknown"),
              "call: unknown operation -> E_NOT_FOUND + error object", e.output());
    }
    try {
        polycall::call(ep, "inventory", "get", "{\"item_id\":\"nope\"}", 3000);
        check(false, "call: unknown item -> E_REMOTE", "no error");
    } catch (const polycall::Error& e) {
        check(e.status() == polycall::status::remote && contains(e.output(), "item.unknown"),
              "call: unknown item -> E_REMOTE + error object", e.output());
    }
    expect_status(polycall::status::timeout,
                  status_of([&] { polycall::call(ep, "debug", "sleep", "{\"ms\":3000}", 200); }, &what),
                  "call: deadline exceeded -> E_TIMEOUT", what);
    expect_status(polycall::status::invalid_argument,
                  status_of([&] { polycall::call(ep, "debug", "echo", "{not json", 1000); }),
                  "call: invalid input JSON -> E_INVALID_ARGUMENT");
    out = polycall::call(ep, "debug", "echo", "", 3000);
    check(out == "{\"echo\":null}", "call: empty input is sent as null", out);

    // timeout_ms boundaries: 1..600000 are valid
    out.clear();
    expect_status(0, status_of([&] { out = polycall::call(ep, "debug", "echo", "600000", 600000); }, &what),
                  "call: timeout_ms 600000 (maximum) accepted", what);
    check(out == "{\"echo\":600000}", "call: output with the maximum timeout", out);
    const int t1 = status_of([&] { polycall::call(ep, "debug", "echo", "1", 1); }, &what);
    check(t1 == 0 || t1 == polycall::status::timeout, "call: timeout_ms 1 (minimum) accepted",
          std::to_string(t1) + " " + what);
    expect_status(polycall::status::invalid_argument,
                  status_of([&] { polycall::call(ep, "debug", "echo", "{}", 600001); }),
                  "call: timeout_ms 600001 -> E_INVALID_ARGUMENT");
    expect_status(polycall::status::invalid_argument,
                  status_of([&] { polycall::call(ep, "debug", "echo", "{}", 0xFFFFFFFFu); }),
                  "call: timeout_ms UINT32_MAX -> E_INVALID_ARGUMENT");

    // concurrent calls from several threads
    constexpr int threads = 4;
    constexpr int per = 10;
    std::atomic<int> good{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            for (int i = 0; i < per; ++i) {
                const std::string in = "{\"t\":" + std::to_string(t) + ",\"i\":" + std::to_string(i) + "}";
                try {
                    if (polycall::call(ep, "debug", "echo", in, 5000) == "{\"echo\":" + in + "}") {
                        ++good;
                    }
                } catch (const polycall::Error&) {
                }
            }
        });
    }
    for (auto& t : pool) {
        t.join();
    }
    check(good == threads * per, "call: 4 threads x 10 concurrent calls, exact outputs",
          std::to_string(good.load()) + " of " + std::to_string(threads * per));
}

void test_daemon_call() {
    const std::string ep = env("POLYCALL_DAEMON_ENDPOINT");
    if (ep.empty()) {
        for (const char* n : {"daemon call: debug.echo", "daemon call: inventory.get",
                              "daemon call: unknown operation", "daemon call: deadline"}) {
            skip(n, "POLYCALL_DAEMON_ENDPOINT not set (run via tests/run-real.sh)");
        }
        return;
    }
    std::string what;
    std::string out = polycall::call(ep, "debug", "echo", "{\"d\":[1,2,\"\xc3\xa9\"]}", 3000);
    check(out == "{\"echo\":{\"d\":[1,2,\"\xc3\xa9\"]}}", "daemon call: debug.echo exact output", out);
    out = polycall::call(ep, "inventory", "get", "{\"item_id\":\"widget-a\"}", 3000);
    check(out == "{\"item_id\":\"widget-a\",\"quantity\":42,\"in_stock\":true}",
          "daemon call: inventory.get exact output", out);
    try {
        polycall::call(ep, "debug", "no_such_op", "{}", 3000);
        check(false, "daemon call: unknown operation -> E_NOT_FOUND", "no error");
    } catch (const polycall::Error& e) {
        check(e.status() == polycall::status::not_found && contains(e.output(), "operation.unknown"),
              "daemon call: unknown operation -> E_NOT_FOUND + error object", e.output());
    }
    expect_status(polycall::status::timeout,
                  status_of([&] { polycall::call(ep, "debug", "sleep", "{\"ms\":3000}", 200); }, &what),
                  "daemon call: deadline exceeded -> E_TIMEOUT", what);
}

// A configuration file whose path is not ASCII (UTF-8, also on Windows).
void test_unicode_config(const std::string& root) {
    namespace fs = std::filesystem;
    // "cpp-polycall-<u-umlaut>nic<o-slash>d<e-acute>-<two CJK characters>"
    const std::string dir = "cpp-polycall-\xc3\xbcnic\xc3\xb8" "d\xc3\xa9-\xe9\x85\x8d\xe7\xbd\xae";
    const std::string file = dir + "/r\xc3\xa9glages-polycallrc";
    const std::string absent = dir + "/absent-\xc3\xa9-polycallrc";
    std::error_code ec;
    fs::remove_all(fs::u8path(dir), ec);
    fs::create_directories(fs::u8path(dir), ec);
    if (!ec) {
        fs::copy_file(fs::u8path(root + "/cpp-polycallrc"), fs::u8path(file),
                      fs::copy_options::overwrite_existing, ec);
    }
    check(!ec, "unicode path: fixture created", ec.message());
    expect_status(0, polycall::run_config(file), "run_config: non-ASCII (UTF-8) config path, run=1");
    expect_status(0, polycall::run_config(file, false), "run_config: non-ASCII (UTF-8) config path, run=0");
    std::string desc;
    std::string what;
    const int ds = status_of([&] { desc = polycall::describe(file); }, &what);
    check(ds == 0 && contains(desc, "log_level"), "describe: non-ASCII (UTF-8) config path",
          what + desc.substr(0, 80));
    try {
        polycall::run_config_or_throw(absent);
        check(false, "run_config: missing non-ASCII path -> E_NOT_FOUND", "no error");
    } catch (const polycall::Error& e) {
        check(e.status() == polycall::status::not_found && contains(e.detail(), "absent-\xc3\xa9-polycallrc"),
              "run_config: missing non-ASCII path -> E_NOT_FOUND, detail keeps the UTF-8 name", e.what());
    }
    fs::remove_all(fs::u8path(dir), ec);
}

// Identifier lengths and caller-buffer capacities at their exact limits.
void test_boundaries() {
    const std::string id63(63, 'n');
    const std::string id64(64, 'n');
    std::string what;
    {
        std::string got;
        const int st = status_of([&] { polycall::Peer p(id63); got = p.node_id(); }, &what);
        check(st == 0 && got == id63, "open: 63-byte node id accepted", what);
    }
    expect_status(polycall::status::invalid_argument, status_of([&] { polycall::Peer p(id64); }),
                  "open: 64-byte node id -> E_INVALID_ARGUMENT");
    polycall::Peer a("cpp-bound-a");
    polycall::Peer b("cpp-bound-b");
    const std::string eb = b.endpoint();
    const std::string mid63(63, 'm');
    a.send(eb, std::string_view("id63"), mid63);
    polycall::Message m = b.recv(5000);
    check(m.id == mid63 && m.text() == "id63", "send: 63-byte message id delivered intact", m.id);
    expect_status(polycall::status::invalid_argument,
                  status_of([&] { a.send(eb, std::string_view("id64"), std::string(64, 'm')); }),
                  "send: 64-byte message id -> E_INVALID_ARGUMENT");
    expect_status(polycall::status::invalid_argument, status_of([&] { a.register_peer(id64, eb); }),
                  "register: 64-byte peer id -> E_INVALID_ARGUMENT");

    // snprintf rules on caller-owned buffers, raw ABI
    char buf[POLYCALL_ENDPOINT_MAX];
    check(polycall_peer_endpoint(b.handle(), buf, eb.size()) == POLYCALL_E_TOO_LARGE,
          "endpoint: capacity strlen -> E_TOO_LARGE");
    check(polycall_peer_endpoint(b.handle(), buf, eb.size() + 1) == POLYCALL_OK && eb == buf,
          "endpoint: capacity strlen+1 -> OK");
    std::size_t need = 0;
    check(polycall_peer_list(b.handle(), buf, 2, &need) == POLYCALL_E_TOO_LARGE && need == 2,
          "list: capacity 2 for \"{}\" -> E_TOO_LARGE, needed 2", std::to_string(need));
    check(polycall_peer_list(b.handle(), buf, 3, &need) == POLYCALL_OK && need == 2 && std::string(buf) == "{}",
          "list: capacity 3 for \"{}\" -> OK");

    // payload_cap exactly the payload size (and one less)
    a.send(eb, std::string_view("exactly-16-bytes"), "m-exact");
    {
        char sender[POLYCALL_PEER_ID_MAX], mid[POLYCALL_MESSAGE_ID_MAX];
        unsigned char pl[16];
        std::size_t len = 0;
        const int s15 = polycall_peer_recv(b.handle(), 5000, sender, sizeof sender, mid, sizeof mid, pl, 15, &len);
        check(s15 == POLYCALL_E_TOO_LARGE && len == 16, "recv: payload_cap = size-1 -> E_TOO_LARGE, needed 16");
        const int s16 = polycall_peer_recv(b.handle(), 0, sender, sizeof sender, mid, sizeof mid, pl, 16, &len);
        check(s16 == POLYCALL_OK && len == 16 &&
                  std::string(reinterpret_cast<char*>(pl), 16) == "exactly-16-bytes",
              "recv: payload_cap = size -> OK, exact bytes");
    }
    // timeout_ms 0 polls
    const auto t0 = std::chrono::steady_clock::now();
    expect_status(polycall::status::timeout, status_of([&] { b.recv(0); }), "recv: timeout 0 polls an empty inbox");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0).count();
    check(ms < 1000, "recv: timeout 0 returns without waiting", std::to_string(ms) + " ms");
}

void exchange(polycall::Peer& from, polycall::Peer& to, const std::string& from_id,
              const std::vector<std::uint8_t>& payload, const std::string& mid,
              const std::string& label) {
    std::string what;
    const int st = status_of([&] { from.send(to.endpoint(), payload, mid); }, &what);
    if (st != 0) {
        check(false, label, "send failed: " + what);
        return;
    }
    polycall::Message m;
    const int rs = status_of([&] { m = to.recv(5000); }, &what);
    check(rs == 0 && m.sender == from_id && m.id == mid && m.payload == payload, label,
          rs ? what : ("sender=" + m.sender + " id=" + m.id + " len=" + std::to_string(m.payload.size())));
}

void test_peers() {
    polycall::Peer a("cpp-a");
    polycall::Peer b("cpp-b");
    const std::string ea = a.endpoint();
    check(ea.rfind("127.0.0.1:", 0) == 0 && ea != "127.0.0.1:0", "peer: ephemeral endpoint", ea);
    check(a.node_id() == "cpp-a", "peer: node_id");
    check(contains(a.health(), "\"node_id\":\"cpp-a\""), "peer: health JSON", a.health());

    // payload matrix, both directions, exact bytes + sender + message id
    std::vector<std::uint8_t> binary = {0x00, 0x01, 0x00, 0xff, 0x7f, 0x00, 0x80};
    std::vector<std::uint8_t> mib(polycall::max_payload);
    for (std::size_t i = 0; i < mib.size(); ++i) {
        mib[i] = static_cast<std::uint8_t>((i * 31u + 7u) & 0xffu);
    }
    const std::vector<std::uint8_t> utf8 = bytes("h\xc3\xa9llo w\xc3\xb6rld \xe2\x9c\x93 \xf0\x9f\x9a\x80");
    exchange(a, b, "cpp-a", {}, "m-empty-ab", "peer a->b: empty payload");
    exchange(b, a, "cpp-b", {}, "m-empty-ba", "peer b->a: empty payload");
    exchange(a, b, "cpp-a", utf8, "m-utf8-ab", "peer a->b: UTF-8 payload");
    exchange(b, a, "cpp-b", utf8, "m-utf8-ba", "peer b->a: UTF-8 payload");
    exchange(a, b, "cpp-a", binary, "m-bin-ab", "peer a->b: binary with NUL");
    exchange(b, a, "cpp-b", binary, "m-bin-ba", "peer b->a: binary with NUL");
    exchange(a, b, "cpp-a", mib, "m-1mib-ab", "peer a->b: exactly 1 MiB");
    exchange(b, a, "cpp-b", mib, "m-1mib-ba", "peer b->a: exactly 1 MiB");
    std::vector<std::uint8_t> over(polycall::max_payload + 1, 0x41);
    std::string what;
    expect_status(polycall::status::too_large,
                  status_of([&] { a.send(b.endpoint(), over, "m-over"); }, &what),
                  "peer: 1 MiB + 1 -> E_TOO_LARGE", what);
    polycall::Message none;
    check(!b.try_recv(none, 200), "peer: oversize payload was never delivered");

    // registry ownership
    a.register_peer("cpp-b", b.endpoint());
    check(contains(a.list(), "\"cpp-b\":\"" + b.endpoint() + "\""), "registry: register on a", a.list());
    check(b.list() == "{}", "registry: b's registry is its own (empty)", b.list());
    a.send("cpp-b", std::string_view("by-id"), "m-by-id");
    polycall::Message m = b.recv(5000);
    check(m.text() == "by-id" && m.sender == "cpp-a", "registry: send by registered id");
    check(b.list() == "{}", "registry: receiving does not register the sender", b.list());
    check(status_of([&] { a.ping("cpp-b"); }) == 0, "ping: registered id answers under its id");
    check(status_of([&] { a.ping(b.endpoint()); }) == 0, "ping: host:port");
    a.register_peer("impostor", b.endpoint());
    expect_status(polycall::status::protocol, status_of([&] { a.ping("impostor"); }, &what),
                  "ping: registered id answered by another node -> E_PROTOCOL", what);
    a.unregister_peer("impostor");
    a.unregister_peer("cpp-b");
    expect_status(polycall::status::not_found, status_of([&] { a.unregister_peer("cpp-b"); }),
                  "registry: unregister unknown -> E_NOT_FOUND");
    check(a.list() == "{}", "registry: unregistered", a.list());
    expect_status(polycall::status::invalid_argument,
                  status_of([&] { a.register_peer("bad id!", b.endpoint()); }),
                  "registry: invalid peer id -> E_INVALID_ARGUMENT");

    // duplicate message id is stored once
    a.send(b.endpoint(), std::string_view("dup"), "m-dup-1");
    a.send(b.endpoint(), std::string_view("dup"), "m-dup-1");
    m = b.recv(5000);
    check(m.id == "m-dup-1" && m.text() == "dup", "duplicate: first copy delivered");
    check(!b.try_recv(none, 300), "duplicate: second copy dropped by the receiver");

    // generated message id
    a.send(b.endpoint(), std::string_view("gen"));
    m = b.recv(5000);
    check(!m.id.empty() && m.text() == "gen", "send: empty message id is generated", m.id);

    // receive timeout
    const auto t0 = std::chrono::steady_clock::now();
    expect_status(polycall::status::timeout, status_of([&] { b.recv(150); }), "recv: timeout -> E_TIMEOUT");
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - t0).count();
    check(ms >= 100, "recv: waited for the timeout", std::to_string(ms) + " ms");

    // too-small buffer leaves the message queued (raw ABI), binding grows it
    a.send(b.endpoint(), std::string_view("0123456789"), "m-small");
    {
        char sender[POLYCALL_PEER_ID_MAX], mid[POLYCALL_MESSAGE_ID_MAX];
        unsigned char tiny[4];
        std::size_t len = 0;
        const int st = polycall_peer_recv(b.handle(), 5000, sender, sizeof sender, mid, sizeof mid,
                                          tiny, sizeof tiny, &len);
        check(st == POLYCALL_E_TOO_LARGE && len == 10, "recv: too-small buffer -> E_TOO_LARGE + needed",
              std::to_string(st) + "/" + std::to_string(len));
        m = b.recv(1000, 4);
        check(m.text() == "0123456789" && m.id == "m-small", "recv: message stayed queued (binding regrows)");
    }

    // send-only node
    polycall::Peer s = polycall::Peer::send_only("cpp-sendonly");
    check(s.endpoint().empty(), "send-only: no endpoint");
    s.send(b.endpoint(), std::string_view("from send-only"), "m-so");
    m = b.recv(5000);
    check(m.sender == "cpp-sendonly" && m.text() == "from send-only", "send-only: can send");
}

void test_auth_and_transport() {
    polycall::Peer secured("cpp-secured", "127.0.0.1:0", "qa-shared-secret-1");
    polycall::Peer anon = polycall::Peer::send_only("cpp-anon");
    polycall::Peer wrong = polycall::Peer::send_only("cpp-wrong", "not-the-secret");
    polycall::Peer member = polycall::Peer::send_only("cpp-member", "qa-shared-secret-1");
    std::string what;
    expect_status(polycall::status::auth,
                  status_of([&] { anon.send(secured.endpoint(), std::string_view("x"), "m-auth-1"); }, &what),
                  "auth: no token -> E_AUTH", what);
    expect_status(polycall::status::auth,
                  status_of([&] { wrong.send(secured.endpoint(), std::string_view("x"), "m-auth-2"); }, &what),
                  "auth: wrong token -> E_AUTH", what);
    member.send(secured.endpoint(), std::string_view("ok"), "m-auth-3");
    polycall::Message m = secured.recv(5000);
    check(m.sender == "cpp-member" && m.id == "m-auth-3", "auth: right token delivers");
    expect_status(polycall::status::config,
                  status_of([] { polycall::Peer p("cpp-open", "0.0.0.0:0"); }),
                  "open: non-loopback bind without token -> E_CONFIG");
    expect_status(polycall::status::invalid_argument,
                  status_of([] { polycall::Peer p("bad id", "127.0.0.1:0"); }),
                  "open: invalid node id -> E_INVALID_ARGUMENT");

    // dead peer: a port that was just released
    std::string dead;
    {
        polycall::Peer gone("cpp-gone");
        dead = gone.endpoint();
    }
    expect_status(polycall::status::transport,
                  status_of([&] { member.send(dead, std::string_view("x"), "m-dead", 3000); }, &what),
                  "send to a dead peer -> E_TRANSPORT", what);
    expect_status(polycall::status::transport, status_of([&] { member.ping(dead, 3000); }),
                  "ping a dead peer -> E_TRANSPORT");
}

void test_cancel_close_handles() {
    polycall::Peer r("cpp-blocked");
    std::atomic<int> got{1};
    std::thread t([&] { got = status_of([&] { r.recv(polycall::wait_forever); }); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    r.cancel();
    t.join();
    expect_status(polycall::status::cancelled, got.load(), "cancel wakes a blocked recv -> E_CANCELLED");

    std::atomic<int> got2{1};
    std::thread t2([&] { got2 = status_of([&] { r.recv(polycall::wait_forever); }); });
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    r.close();
    t2.join();
    expect_status(polycall::status::closed, got2.load(), "close wakes a blocked recv -> E_CLOSED");

    std::string what;
    expect_status(polycall::status::invalid_handle, status_of([&] { r.close(); }, &what),
                  "double close -> E_INVALID_HANDLE", what);
    expect_status(polycall::status::invalid_handle, status_of([&] { (void)r.endpoint(); }),
                  "endpoint after close -> E_INVALID_HANDLE");
    expect_status(polycall::status::invalid_handle,
                  status_of([&] { r.send("127.0.0.1:1", std::string_view("x")); }),
                  "send after close -> E_INVALID_HANDLE");
    expect_status(polycall::status::invalid_handle, status_of([&] { r.recv(0); }),
                  "recv after close -> E_INVALID_HANDLE");
    check(polycall_peer_close(0) == POLYCALL_E_INVALID_HANDLE &&
              polycall_peer_close(-7) == POLYCALL_E_INVALID_HANDLE &&
              polycall_peer_ping(987654, "127.0.0.1:1", 100) == POLYCALL_E_INVALID_HANDLE,
          "raw invalid handles -> E_INVALID_HANDLE");

    // moved-from object no longer owns the node
    polycall::Peer p1("cpp-move");
    const std::int32_t h = p1.handle();
    polycall::Peer p2(std::move(p1));
    check(p2.handle() == h && !p1.is_open() && p2.is_open(), "Peer is move-only RAII");
}

void test_concurrent_senders() {
    polycall::Peer rx("cpp-rx");
    const std::string ep = rx.endpoint();
    constexpr int threads = 4;
    constexpr int per = 25;
    std::atomic<int> failures{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t) {
        pool.emplace_back([&, t] {
            try {
                polycall::Peer tx = polycall::Peer::send_only("cpp-tx" + std::to_string(t));
                for (int i = 0; i < per; ++i) {
                    tx.send(ep, "t" + std::to_string(t) + "-" + std::to_string(i),
                            "m-" + std::to_string(t) + "-" + std::to_string(i));
                }
            } catch (const polycall::Error&) {
                ++failures;
            }
        });
    }
    std::set<std::string> seen;
    for (int i = 0; i < threads * per; ++i) {
        polycall::Message m;
        if (!rx.try_recv(m, 10000)) {
            break;
        }
        seen.insert(m.sender + "/" + m.id);
    }
    for (auto& t : pool) {
        t.join();
    }
    check(failures == 0 && seen.size() == static_cast<std::size_t>(threads * per),
          "concurrent senders: 4 threads x 25 messages, all delivered once",
          std::to_string(seen.size()) + " unique, " + std::to_string(failures.load()) + " send failures");
}

void test_cli_interop() {
    const std::string cli = env("POLYCALL_CLI");
    const std::string cli_peer = env("POLYCALL_CLI_PEER");
    const std::string token = env("POLYCALL_DEV_TOKEN");
    if (cli.empty() || cli_peer.empty()) {
        skip("interop: polycall CLI peer -> cpp", "POLYCALL_CLI/POLYCALL_CLI_PEER not set (run via tests/run-real.sh)");
        skip("interop: cpp -> polycall CLI peer", "POLYCALL_CLI/POLYCALL_CLI_PEER not set (run via tests/run-real.sh)");
        return;
    }
    static const char raw[] = "cpp\0bin\x01\xff \xc3\xa9\xe2\x9c\x93 end";
    const std::vector<std::uint8_t> payload = bytes(std::string(raw, sizeof raw - 1));
    polycall::Peer node("cpp-node", "127.0.0.1:0", token);

    // CLI -> binding
    {
        std::ofstream f("cpp_cli_payload.bin", std::ios::binary);
        f.write(reinterpret_cast<const char*>(payload.data()), static_cast<std::streamsize>(payload.size()));
    }
    const int rc = run_command("\"" + cli + "\" peer send --to " + node.endpoint() +
                               " --payload-file cpp_cli_payload.bin --from cli-node --id cli-to-cpp-1"
                               " -t 5000 > cpp_cli_send.log 2>&1");
    polycall::Message m;
    std::string what;
    const int st = rc == 0 ? status_of([&] { m = node.recv(5000); }, &what) : -1;
    check(rc == 0 && st == 0 && m.sender == "cli-node" && m.id == "cli-to-cpp-1" && m.payload == payload,
          "interop: polycall CLI peer send -> cpp Peer (bytes, sender, id)",
          "rc=" + std::to_string(rc) + " " + what + " " + slurp("cpp_cli_send.log"));

    // binding -> CLI node (registered by id, identity checked by ping)
    node.register_peer("cli-node", cli_peer);
    check(status_of([&] { node.ping("cli-node"); }, &what) == 0, "interop: ping the CLI node by id", what);
    const int ss = status_of([&] { node.send("cli-node", payload, "cpp-to-cli-1"); }, &what);
    const int rc2 = run_command("\"" + cli + "\" --format json peer recv --to " + cli_peer +
                                " -t 5000 > cpp_cli_recv.json 2>&1");
    const std::string got = slurp("cpp_cli_recv.json");
    check(ss == 0 && rc2 == 0 && contains(got, "\"from\":\"cpp-node\"") &&
              contains(got, "\"id\":\"cpp-to-cli-1\"") &&
              contains(got, "\"payload_b64\":\"" + base64(payload) + "\""),
          "interop: cpp Peer -> polycall peer serve, read by polycall peer recv", what + " " + got);
    std::remove("cpp_cli_payload.bin");
    std::remove("cpp_cli_send.log");
    std::remove("cpp_cli_recv.json");
}

} // namespace

int main(int argc, char** argv) {
    const std::string root = argc > 1 ? argv[1] : ".";
    std::printf("cpp-polycall real-core tests: libpolycall %s, ABI %d\n",
                polycall::version().c_str(), polycall::abi_version());
    const auto section = [](const char* name, void (*fn)()) {
        std::string what;
        if (status_of(fn, &what) != 0) {
            check(false, std::string(name) + ": unexpected exception", what);
        }
    };
    {
        std::string what;
        if (status_of([&] { test_run_config(root); }, &what) != 0) {
            check(false, "run_config: unexpected exception", what);
        }
    }
    section("version", test_version_and_abi);
    section("call", test_call);
    section("daemon call", test_daemon_call);
    {
        std::string what;
        if (status_of([&] { test_unicode_config(root); }, &what) != 0) {
            check(false, "unicode path: unexpected exception", what);
        }
    }
    section("boundaries", test_boundaries);
    section("peers", test_peers);
    section("auth/transport", test_auth_and_transport);
    section("cancel/close/handles", test_cancel_close_handles);
    section("concurrency", test_concurrent_senders);
    section("interop", test_cli_interop);
    std::printf("SUMMARY pass=%d fail=%d skip=%d\n", g_pass, g_fail, g_skip);
    if (g_fail != 0) {
        return 1;
    }
    return g_skip != 0 ? 77 : 0; // 77: CTest SKIP -- skipped checks are never a pass
}
