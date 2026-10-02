#!/usr/bin/env sh
# Thin-adapter audit: cpp-polycall must reach the core only through the
# binding ABI (<polycall.h>) and must not re-implement configuration parsing,
# sockets or the peer protocol.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
src="$root/src/polycall.cpp"

if grep -E -n 'fopen|ifstream|CreateFile|sscanf|strtok|socket\(|connect\(|recvfrom|getaddrinfo' "$src"; then
    echo "cpp-polycall must not parse configuration or implement runtime logic" >&2
    exit 1
fi
if grep -R -n 'polycall_ffi\.h' "$root/src" "$root/include" "$root/CMakeLists.txt" "$root/Makefile"; then
    echo "cpp-polycall must include the real <polycall.h>, not a generated stub" >&2
    exit 1
fi

grep -F -q '#include <polycall.h>' "$src"
grep -F -q 'polycall_ffi_run_config(config_path.c_str(), strict ? 1 : 0)' "$src"
grep -F -q 'polycall_ffi_abi_version() == expected_abi_version' "$src"

echo "cpp-polycall thin-adapter check: PASS"
