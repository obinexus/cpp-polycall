$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$sourcePath = Join-Path $root 'src/polycall.cpp'
$source = Get-Content -Raw $sourcePath
$forbidden = 'fopen|open\(|CreateFile|sscanf|strtok|socket\(|connect\('
$matches = Select-String -Path $sourcePath -Pattern $forbidden

if ($matches) {
    $matches | ForEach-Object { Write-Error $_.Line }
    throw 'cpp-polycall must not parse configuration or implement runtime logic'
}

if (-not $source.Contains('polycall_ffi_run_config(config_path.c_str(), 1)')) {
    throw 'cpp-polycall does not forward through polycall_ffi_run_config'
}

Write-Output 'cpp-polycall thin-adapter check: PASS'
