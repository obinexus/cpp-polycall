# cpp-polycall

Idiomatic C++17 binding for libpolycall 1.5.0, published as
`@obinexusltd/cpp-polycall`.

The adapter maps C++ strings and exceptions onto the single core FFI call
`polycall_ffi_run_config(config_path, 1)`. It contains no configuration parser,
context implementation, or duplicated runtime logic.

## C++ API

Choose explicit status handling:

```cpp
#include <cpp_polycall/polycall.hpp>

const int status = polycall::run_config("cpp-polycallrc");
if (status != 0) {
    // Apply the application's error policy.
}
```

Or use the exception-oriented API:

```cpp
try {
    polycall::run_config_or_throw("cpp-polycallrc");
} catch (const polycall::Error& error) {
    std::cerr << error.what() << '\n';
    return error.code();
}
```

`run_config` returns the core status unchanged. `run_config_or_throw` raises
`polycall::Error`, whose `code()` and `status()` accessors expose that same
value. Calling either function without an argument uses `cpp-polycallrc`.

The original `src/polycall.hpp` include path remains as a compatibility shim.
New code should include `<cpp_polycall/polycall.hpp>`.

## Install from npm

```sh
npm install @obinexusltd/cpp-polycall
```

This is a native source package. Its CommonJS entry point exposes absolute
paths for C++ build tooling:

```js
const polycall = require('@obinexusltd/cpp-polycall');

console.log(polycall.source);
console.log(polycall.header);
console.log(polycall.compatibilityHeader);
console.log(polycall.ffiHeader);
console.log(polycall.cmakeLists);
console.log(polycall.config);
```

The npm tarball includes C++ sources, headers, CMake package files, examples,
tests, scripts, Makefile, manifest, and runtime configuration. Generated native
binaries are excluded.

## Build and test

Using Make/npm:

```sh
npm run build
npm test
npm run verify
```

Using CMake and CTest:

```sh
cmake -S . -B cmake-build -DBUILD_TESTING=ON
cmake --build cmake-build
ctest --test-dir cmake-build --output-on-failure
```

The tests link the real C++ adapter against a mock libpolycall FFI and verify
the path, `run=1`, status propagation, and exception contract.

## CMake consumption

Install the package:

```sh
cmake --install cmake-build --prefix /desired/prefix
```

Then link both the adapter and libpolycall:

```cmake
find_package(cpp_polycall 1 CONFIG REQUIRED)
find_package(polycall 1.5 CONFIG REQUIRED)

target_link_libraries(my_app PRIVATE
    cpp_polycall::cpp_polycall
    polycall::polycall)
```

The static C++ adapter intentionally leaves `polycall_ffi_run_config` for the
libpolycall v1.5 library to resolve at final link time.

## Run the example

```sh
make example POLYCALL_LDFLAGS="-L/path/to/libpolycall/lib -lpolycall"
```

The example accepts an optional configuration path and otherwise uses
`cpp-polycallrc`.

## Publishing

```sh
npm pack --dry-run
npm publish --access public
```

Publishing is not performed automatically.

## Author

Nnamdi Michael Okpala — <okpalan@protonmail.com>
