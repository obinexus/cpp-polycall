# cpp-polycall

C++17 RAII binding for the [Polycall](https://github.com/obinexus/polycall)
core, published as the npm source package `cpp-polycall`.

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

Windows, MSVC (x64) against an installed `polycall.dll` / `polycall.lib`:

```powershell
cmake -S . -B cmake-build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_PREFIX_PATH=C:\path\to\polycall -DBUILD_TESTING=ON
cmake --build cmake-build --config Release
ctest --test-dir cmake-build -C Release --output-on-failure
```

Windows, MSYS2 UCRT64 (MinGW-w64 GCC) against an installed `libpolycall.dll`
(from a UCRT64 shell, or with `C:\msys64\ucrt64\bin` first on `PATH`):

```sh
cmake -S . -B cmake-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=C:/path/to/polycall -DBUILD_TESTING=ON
cmake --build cmake-build && ctest --test-dir cmake-build --output-on-failure
```

CTest runs two tests:

* `cpp_polycall_real_core` -- `tests/real_core_test.cpp` against the real
  library, through `tests/run-real.sh`, which first starts `polycall peer
  serve`, `polycall start` and `polycall daemon start` on ephemeral loopback
  ports (private state directory), so cross-language interop and RPC against
  both runtimes are exercised. On Windows a plain MSYS `sh` runs the script
  (MSYS2's or Git's `usr/bin/sh.exe`). Without the CLI those checks print
  `SKIP` and the test exits 77, which CTest reports as *Skipped* -- never as
  passed.
* `cpp_polycall_loader_errors` -- `tests/loader-errors.sh`: the program
  against the real library, no library, a library reporting ABI 2 and a 1.0
  library without the ABI v1 symbols (the last two are test fixtures built
  from `tests/loader/fake_polycall.cpp`).

See [tests/TESTS.md](tests/TESTS.md). Tested against polycall 1.1.0 on Linux
(Debian 13: GCC 14 and Clang 19; valgrind, ASan+UBSan and TSan clean) and on
Windows x64 (MSVC 19.44 / VS 2022, and MSYS2 UCRT64 GCC with CMake and with
the Makefile).

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

`tests/consumer/` is such a project; it is built against an installed copy
(`cmake --install`) in QA.

## Loading the library

The binding links the core at build time (`polycall::polycall` or
`pkg-config polycall`), so the platform loader checks the library before any
binding code runs:

| Situation | Result |
| --- | --- |
| no library at link time | link error |
| no library at run time | the loader refuses to start the program -- Linux: exit 127, `error while loading shared libraries: libpolycall.so.1: cannot open shared object file`; Windows: `polycall.dll` / `libpolycall.dll` not found (`STATUS_DLL_NOT_FOUND`) |
| 1.0 library without the ABI v1 symbols | the loader refuses -- Linux: exit 127, `symbol lookup error: ... undefined symbol: polycall_...`; Windows: `STATUS_ENTRYPOINT_NOT_FOUND` (0xC0000139) |
| library reporting another binding ABI | `polycall::check_abi()` -- also run before the first ABI call of every entry point -- throws `polycall::Error` (`POLYCALL_E_UNSUPPORTED`, naming both ABI versions); `run_config()` returns `POLYCALL_E_UNSUPPORTED` |

## npm source package

```sh
npm install cpp-polycall
```

The CommonJS entry point only exposes absolute paths for C++ build tooling
(`source`, `header`, `cmakeLists`, `config`, ...). It does not load
libpolycall. The package is not yet published.

## Author

Nnamdi Michael Okpala — <okpalan@protonmail.com>. MIT licensed, see
[LICENSE](LICENSE).
