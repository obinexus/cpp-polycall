# TODO — cpp-polycall (C++)

Status: supported -- C++17 RAII wrapper over Polycall binding ABI v1
(polycall >= 1.1.0).

- [x] `run_config` / `run_config_or_throw` kept, plus strict/validate mode
- [x] Version + ABI check, `describe`, `call`, RAII `Peer`, `polycall::Error`
- [x] Links the installed core via CMake (`polycall::polycall`) or pkg-config
- [x] Real-core tests incl. interop with the C CLI, `polycall start` and `polycall daemon` (Linux GCC/Clang, Windows MSVC and MSYS2 UCRT64)
- [x] Loader-error test (no library, 1.0 library, ABI mismatch), non-ASCII config path, limits, concurrent calls
- [x] valgrind, ASan+UBSan, TSan clean (Linux)
- [x] npm source package metadata
- [ ] Publish `@obinexusltd/cpp-polycall` (not published yet)
