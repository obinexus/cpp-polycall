# TODO — cpp-polycall (C++)

Status: supported -- C++17 RAII wrapper over Polycall binding ABI v1
(polycall >= 1.1.0).

- [x] `run_config` / `run_config_or_throw` kept, plus strict/validate mode
- [x] Version + ABI check, `describe`, `call`, RAII `Peer`, `polycall::Error`
- [x] Links the installed core via CMake (`polycall::polycall`) or pkg-config
- [x] Real-core tests incl. interop with the C CLI (Linux GCC, Windows MSVC)
- [x] npm source package metadata
- [ ] Publish `@obinexusltd/cpp-polycall` (not published yet)
