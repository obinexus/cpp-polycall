#!/usr/bin/env sh
# Loader behaviour of the link-time binding (CTest: cpp_polycall_loader_errors).
#
#   tests/loader-errors.sh <abi_guard_test> <real lib> <fake ABI-2 lib> <fake 1.0 lib>
#
# cpp-polycall links libpolycall at build time, so the platform loader is
# the first line of defence and polycall::check_abi() the second:
#   1. real library             -> ABI 1 accepted
#   2. no library               -> the loader refuses to start the program
#                                  (Linux: exit 127 "libpolycall.so.1: cannot open
#                                  shared object file"; Windows: polycall.dll /
#                                  libpolycall.dll not found) -- never a crash
#   3. library reporting ABI 2  -> every entry point throws polycall::Error
#                                  (E_UNSUPPORTED) before calling the library
#   4. 1.0 library (no ABI v1)  -> the loader refuses (undefined symbol /
#                                  entry point not found) -- never a crash
# The fake libraries are TEST FIXTURES (tests/loader/fake_polycall.cpp).
set -u
if [ "$#" -ne 4 ]; then
    echo "usage: $0 <abi_guard_test> <real lib> <fake ABI-2 lib> <fake 1.0 lib>" >&2
    exit 2
fi
guard=$1 real=$2 abi2=$3 old=$4
# scratch space in the working directory (CTest: the build tree)
tmp="$(pwd)/.loader-errors.$$"
rm -rf "$tmp"
mkdir "$tmp" || exit 1
trap 'rm -rf "$tmp"' EXIT
pass=0 fail=0 skip=0
ok()   { pass=$((pass + 1)); echo "PASS $1"; }
bad()  { fail=$((fail + 1)); echo "FAIL $1"; [ -f "$2" ] && sed 's/^/    | /' "$2"; }
skp()  { skip=$((skip + 1)); echo "SKIP $1"; }

case "$(uname -s)" in
    MINGW* | MSYS* | CYGWIN*) windows=1 ;;
    *) windows= ;;
esac

# run_case <name> <library file or ""> <guard mode> -> sets rc, log=$tmp/<name>.log
run_case() {
    name=$1 lib=$2 mode=$3
    log="$tmp/$name.log"
    dir="$tmp/$name"
    mkdir -p "$dir"
    if [ -n "$windows" ]; then
        # Windows searches the program's own directory first.
        cp "$guard" "$dir/"
        [ -n "$lib" ] && cp "$lib" "$dir/"
        (cd "$dir" && "./$(basename "$guard")" "$mode") >"$log" 2>&1
        rc=$?
    else
        if [ -n "$lib" ]; then
            libdir=$(cd "$(dirname "$lib")" && pwd)
        else
            libdir=$dir
        fi
        LD_LIBRARY_PATH=$libdir "$guard" "$mode" >"$log" 2>&1
        rc=$?
    fi
}

run_case real "$real" expect-ok
if [ "$rc" -eq 0 ]; then ok "real library: ABI 1 accepted"; else bad "real library (exit $rc)" "$log"; fi

run_case none "" expect-ok
if [ "$rc" -eq 0 ]; then
    skp "no library: libpolycall is on the default loader path, cannot hide it"
elif [ "$rc" -gt 128 ] && [ -z "$windows" ]; then
    bad "no library: crashed (exit $rc)" "$log"
elif grep -Eqi 'libpolycall\.so\.1: cannot open shared object|polycall\.dll|cannot open shared object|not found' "$log"; then
    ok "no library: loader refuses with a clear message (exit $rc): $(head -1 "$log" | tr -d '\r')"
else
    bad "no library: exit $rc without a loader message" "$log"
fi

run_case abi2 "$abi2" expect-mismatch
if [ "$rc" -eq 0 ]; then ok "ABI 2 library: every entry point refuses with polycall::Error(E_UNSUPPORTED)"
else bad "ABI 2 library (exit $rc)" "$log"; fi

run_case old "$old" expect-ok
if [ -n "$windows" ] && [ "$rc" -eq 127 ] && ! grep -Eqi 'entry point|procedure|not found' "$log"; then
    # MSYS reports a failed process start as exit 127 without text; ask
    # Windows for the real NTSTATUS of the same program.
    nt=$(powershell.exe -NoProfile -NonInteractive -Command \
        "\$p = Start-Process -FilePath '$(cygpath -w "$tmp/old/$(basename "$guard")")' -ArgumentList 'expect-ok' -NoNewWindow -Wait -PassThru; '{0:X}' -f \$p.ExitCode" \
        2>/dev/null | tr -d '\r\n')
    if [ "$nt" = "C0000139" ]; then
        echo "Windows loader: STATUS_ENTRYPOINT_NOT_FOUND (0xC0000139) -- an ABI v1 entry point is missing from the DLL" >>"$log"
    fi
fi
if [ "$rc" -eq 0 ]; then
    bad "1.0 library: the program ran against a library without the ABI v1 symbols" "$log"
elif [ "$rc" -gt 128 ] && [ -z "$windows" ]; then
    bad "1.0 library: crashed (exit $rc)" "$log"
elif grep -Eqi 'undefined symbol: polycall_|entry point|procedure|not found|cannot open' "$log"; then
    ok "1.0 library: loader refuses with a clear message (exit $rc): $(grep -Ei 'undefined symbol|entry point|procedure|not found|cannot open' "$log" | head -1 | tr -d '\r')"
else
    bad "1.0 library: exit $rc without a loader message" "$log"
fi

echo "SUMMARY pass=$pass fail=$fail skip=$skip"
[ "$fail" -eq 0 ] || exit 1
[ "$skip" -eq 0 ] || exit 77
exit 0
