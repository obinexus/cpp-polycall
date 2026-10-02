# cpp-polycall

C++17 RAII binding for the [Polycall](https://github.com/obinexus/polycall)
core, published as the npm source package `@obinexusltd/cpp-polycall`.

It wraps the core's **binding ABI v1** (`<polycall.h>`, `docs/BINDING_ABI.md`
in the core repository) and requires **polycall >= 1.1.0**. The wrapper owns
no configuration parser, socket code or protocol logic: every operation is
one call into libpolycall. Errors become `polycall::Error` exceptions; peer
nodes are move-only RAII objects.

## C++ API

```cpp
#include <cpp_polycall/polycall.hpp>

polycall::check_abi();                       // throws unless the library speaks ABI 1
std::string v = polycall::version();         // "1.1.0"

// Configuration (never starts a service, never touches the network)
int st = polycall::run_config("cpp-polycallrc");          // polycall_ffi_run_config(path, 1)
st = polycall::run_config("cpp-polycallrc", /*strict=*/false);
polycall::run_config_or_throw("cpp-polycallrc");           // throws polycall::Error
std::string json = polycall::describe("cpp-polycallrc");

// One RPC round trip to `polycall start` / `polycall daemon start`
std::string out = polycall::call("127.0.0.1:7000", "inventory", "get",
                                 R"({"item_id":"widget-a"})", 2000);

// Peer nodes (NSIGII-style, no broker)
polycall::Peer alpha("alpha");                         // listens on 127.0.0.1:<ephemeral>
polycall::Peer beta("beta", "127.0.0.1:0", token);     // with a shared token
auto sender = polycall::Peer::send_only("gamma");      // no listener
alpha.register_peer("beta", beta.endpoint());
alpha.ping("beta");
alpha.send("beta", std::string_view("hello"), "msg-1");   // exactly one delivery attempt
polycall::Message m = beta.recv(5000);                 // m.sender, m.id, m.payload (bytes)
beta.cancel();                                         // wakes a blocked recv (E_CANCELLED)
alpha.close();                                         // explicit; destructor closes otherwise
```

`polycall::Error` carries `status()` (the negative `POLYCALL_E_*` code, also
`code()`), `name()` (`polycall_strerror`), `detail()` (`polycall_last_error`,
captured on the failing thread) and, for `call`, `output()` -- the remote
error object `{"code":..,"message":..}`. Status constants live in
`polycall::status` (`polycall::status::timeout`, ...).

`run_config` keeps its historical behaviour: it returns the core status
unchanged and defaults to `cpp-polycallrc` with `run=1`.

## Build and test

The core is located through its CMake package (`polycall::polycall`) or
`pkg-config polycall`.

```sh
cmake -S . -B cmake-build -DCMAKE_PREFIX_PATH=/opt/polycall -DBUILD_TESTING=ON
cmake --build cmake-build
ctest --test-dir cmake-build --output-on-failure
```

Windows (MSVC, x64) against an installed `polycall.dll` / `polycall.lib`:

```powershell
cmake -S . -B cmake-build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH=C:\path\to\polycall -DBUILD_TESTING=ON
cmake --build cmake-build --config Release
ctest --test-dir cmake-build -C Release --output-on-failure
```

The test (`tests/real_core_test.cpp`) runs against the real library; CTest
runs it through `tests/run-real.sh`, which starts `polycall peer serve` and
`polycall start` so cross-language interop and RPC are exercised too (Git
Bash's `sh` is used on Windows). Without the CLI those checks report `SKIP`,
never `PASS`. See [tests/TESTS.md](tests/TESTS.md).

GNU Make alternative (Linux / MSYS2):

```sh
export PKG_CONFIG_PATH=/opt/polycall/lib/pkgconfig LD_LIBRARY_PATH=/opt/polycall/lib
make && make test
```

## CMake consumption

```cmake
find_package(cpp_polycall 1 CONFIG REQUIRED)   # pulls in find_package(polycall 1.1)
target_link_libraries(my_app PRIVATE cpp_polycall::cpp_polycall)
```

The binding links the core at build time, so a missing core is a link error
and an old 1.0 library (without the ABI v1 symbols) fails at load time;
`polycall::check_abi()` (also run before the first ABI call) rejects a
library whose `polycall_ffi_abi_version()` is not 1 with a `polycall::Error`.

## npm source package

```sh
npm install @obinexusltd/cpp-polycall
```

The CommonJS entry point only exposes absolute paths for C++ build tooling
(`source`, `header`, `cmakeLists`, `config`, ...). It does not load
libpolycall. The package is not yet published.

## Author

Nnamdi Michael Okpala — <okpalan@protonmail.com>. MIT licensed, see
[LICENSE](LICENSE).
