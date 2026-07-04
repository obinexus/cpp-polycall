#!/usr/bin/env sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

if grep -E -n 'fopen|open\(|CreateFile|sscanf|strtok|socket\(|connect\(' \
    "$root/src/polycall.cpp"; then
    echo "cpp-polycall must not parse configuration or implement runtime logic" >&2
    exit 1
fi

grep -F -q 'polycall_ffi_run_config(config_path.c_str(), 1)' \
    "$root/src/polycall.cpp"

echo "cpp-polycall thin-adapter check: PASS"
