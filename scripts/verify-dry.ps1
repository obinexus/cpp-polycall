$ErrorActionPreference = 'Stop'

# Thin-adapter audit (see verify-dry.sh).
$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'src/polycall.cpp'
$source = Get-Content -Raw $sourcePath
$forbidden = 'fopen|ifstream|CreateFile|sscanf|strtok|socket\(|connect\(|recvfrom|getaddrinfo'
$found = Select-String -Path $sourcePath -Pattern $forbidden

if ($found) {
    $found | ForEach-Object { Write-Error $_.Line }
    throw 'cpp-polycall must not parse configuration or implement runtime logic'
}

$stub = Get-ChildItem -Recurse -File (Join-Path $root 'src'), (Join-Path $root 'include') |
    Select-String -SimpleMatch 'polycall_ffi.h'
if ($stub) {
    throw 'cpp-polycall must include the real <polycall.h>, not a generated stub'
}

foreach ($token in @(
        '#include <polycall.h>',
        'polycall_ffi_run_config(config_path.c_str(), strict ? 1 : 0)',
        'polycall_ffi_abi_version() == expected_abi_version')) {
    if (-not $source.Contains($token)) {
        throw "cpp-polycall is missing required ABI forwarding token: $token"
    }
}

Write-Output 'cpp-polycall thin-adapter check: PASS'
