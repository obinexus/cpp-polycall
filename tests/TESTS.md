# cpp-polycall tests

All functional tests run against the REAL installed Polycall core (no mock):

- `real_core_test.cpp` -- the checklist of the core's `docs/BINDING_ABI.md`:
  version/ABI check; `run_config` (valid, missing, malformed, strict unknown
  key, unsupported TLS) and `describe`, also for a non-ASCII (UTF-8) config
  path; `polycall::call` against a live `polycall start` runtime (success,
  unknown operation, unknown item, deadline, invalid input, no runtime,
  `timeout_ms` 1 / 600000 accepted and 600001 / `UINT32_MAX` rejected,
  4 threads x 10 concurrent calls) and against `polycall daemon start`
  (success, unknown operation, deadline); two `Peer` nodes exchanging empty,
  UTF-8, binary-with-NUL and exactly-1-MiB payloads in both directions
  (bytes, sender and message id verified), 1 MiB + 1 rejected; registry
  ownership; duplicate message id stored once; auth failure; dead peer
  (`E_TRANSPORT`); receive timeout and `timeout 0` poll; too-small buffer
  keeps the message queued; 63/64-byte node, peer and message ids;
  snprintf-rule buffer capacities (strlen vs strlen+1, payload_cap = size-1
  vs size); cancel and close waking a blocked `recv`; double close / use
  after close / invalid handles; 4 concurrent sender threads; interop both
  ways with the C CLI (`polycall peer send` -> C++ peer, C++ peer ->
  `polycall peer serve`, read back with `polycall peer recv`). Checks use
  `check()`, not `assert()`, so they also fail in NDEBUG builds. Exit status:
  0 all passed, 1 a check failed, 77 (CTest SKIP) a check was skipped.
- `run-real.sh` starts `polycall peer serve`, `polycall start` and
  `polycall daemon start` (scratch Polycallfile, private state directory) on
  ephemeral loopback ports with a random `POLYCALL_DEV_TOKEN`, then runs the
  test and stops them. Without the CLI it exits 77 (CTest SKIP).
- `loader-errors.sh` + `abi_guard_test.cpp` -- loader behaviour of the
  link-time binding: real library accepted; no library and a 1.0 library
  (no ABI v1 symbols) refused by the platform loader with a clear message,
  never a crash; a library reporting ABI 2 refused by `polycall::check_abi()`
  in every entry point. `loader/fake_polycall.cpp` builds the two fake
  libraries -- TEST FIXTURES, not the core.
- `consumer/` -- packaging check: a separate CMake project using an installed
  cpp-polycall through `find_package(cpp_polycall)`.
- `package.test.js` -- npm entry point paths.

```sh
cmake -S . -B cmake-build -DCMAKE_PREFIX_PATH=/opt/polycall -DBUILD_TESTING=ON
cmake --build cmake-build
ctest --test-dir cmake-build --output-on-failure
```
