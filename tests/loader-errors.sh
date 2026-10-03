#!/usr/bin/env sh
# Loader behaviour of a link-time Polycall binding.
#
#   tests/loader-errors.sh <program> <real lib> <fake ABI-2 lib> <fake 1.0 lib>
#
# <program> links libpolycall at build time and takes one argument,
# `expect-ok` or `expect-mismatch` (exit 0 when its own checks pass). The
# platform loader is the first line of defence, the binding's ABI check the
# second:
#   1. real library             -> ABI 1 accepted
#   2. no library               -> the loader refuses to start the program:
#                                  Linux exit 127 "libpolycall.so.1: cannot open
#                                  shared object file"; Windows
#                                  STATUS_DLL_NOT_FOUND -- never a crash
#   3. library reporting ABI 2  -> refused by the binding's ABI check
#   4. 1.0 library (no ABI v1)  -> the loader refuses: Linux exit 127
#                                  "undefined symbol: polycall_..."; Windows
#                                  STATUS_ENTRYPOINT_NOT_FOUND -- never a crash
# The fake libraries are TEST FIXTURES (tests/loader/), not the core.
# Exit: 0 all passed, 1 a check failed, 77 a check could not run.
set -u
if [ "$#" -ne 4 ]; then
    echo "usage: $0 <program> <real lib> <fake ABI-2 lib> <fake 1.0 lib>" >&2
    exit 2
fi
prog=$1 real=$2 abi2=$3 old=$4
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
    MINGW* | MSYS* | CYGWIN*) windows=1; ps=$(command -v powershell.exe || true) ;;
    *) windows= ps= ;;
esac

# PATH without the directories that hold a Polycall DLL (Windows searches
# PATH after the program directory; each case must see only its own DLL).
clean_path() {
    out="" saved_ifs=$IFS
    IFS=:
    for d in $PATH; do
        [ -n "$d" ] || continue
        if [ -e "$d/libpolycall.dll" ] || [ -e "$d/polycall.dll" ]; then continue; fi
        out="${out:+$out:}$d"
    done
    IFS=$saved_ifs
    printf "%s" "$out"
}

# Windows: the NTSTATUS a failed process start ended with (MSYS only says
# exit 127, and its "cannot open shared object" text may name the wrong DLL).
nt_status() { # <dir> <program file> <mode>
    [ -n "$ps" ] || return 0
    PATH=$(clean_path) "$ps" -NoProfile -NonInteractive -Command \
        "\$p = Start-Process -FilePath '$(cygpath -w "$1/$2")' -ArgumentList '$3' -WorkingDirectory '$(cygpath -w "$1")' -NoNewWindow -Wait -PassThru; '{0:X}' -f \$p.ExitCode" \
        2>/dev/null | tr -d '\r\n'
}

# run_case <name> <library file or ""> <mode> -> sets rc, nt, log
run_case() {
    name=$1 lib=$2 mode=$3
    log="$tmp/$name.log"
    dir="$tmp/$name"
    nt=""
    mkdir -p "$dir"
    if [ -n "$windows" ]; then
        # Windows searches the program's own directory first.
        cp "$prog" "$dir/"
        [ -n "$lib" ] && cp "$lib" "$dir/"
        (cd "$dir" && PATH=$(clean_path) && "./$(basename "$prog")" "$mode") >"$log" 2>&1
        rc=$?
        if [ "$rc" -ne 0 ]; then
            nt=$(nt_status "$dir" "$(basename "$prog")" "$mode")
            echo "Windows process exit status: 0x$nt" >>"$log"
        fi
    else
        if [ -n "$lib" ]; then
            libdir=$(cd "$(dirname "$lib")" && pwd)
        else
            libdir=$dir
        fi
        LD_LIBRARY_PATH=$libdir "$prog" "$mode" >"$log" 2>&1
        rc=$?
    fi
}

first() { grep -Ei "$1" "$log" | head -1 | tr -d '\r'; }

run_case real "$real" expect-ok
if [ "$rc" -eq 0 ]; then ok "real library: ABI 1 accepted"; else bad "real library (exit $rc)" "$log"; fi

run_case none "" expect-ok
if [ "$rc" -eq 0 ]; then
    skp "no library: a libpolycall is still on the default loader path, cannot hide it"
elif [ -n "$windows" ] && [ "$nt" = "C0000135" ]; then
    ok "no library: Windows loader refuses to start the program (STATUS_DLL_NOT_FOUND 0xC0000135)"
elif [ -z "$windows" ] && [ "$rc" -eq 127 ] && grep -q 'libpolycall\.so\.1: cannot open shared object' "$log"; then
    ok "no library: loader refuses (exit 127): $(first 'cannot open shared object')"
else
    bad "no library: exit $rc (status 0x$nt) without the expected loader failure" "$log"
fi

run_case abi2 "$abi2" expect-mismatch
if [ "$rc" -eq 0 ]; then ok "ABI 2 library: refused by the binding's ABI check ($(first 'ABI 2|refus|unsupported'))"
else bad "ABI 2 library (exit $rc)" "$log"; fi

run_case old "$old" expect-ok
if [ "$rc" -eq 0 ]; then
    bad "1.0 library: the program ran against a library without the ABI v1 symbols" "$log"
elif [ -n "$windows" ] && [ "$nt" = "C0000139" ]; then
    ok "1.0 library: Windows loader refuses to start the program (STATUS_ENTRYPOINT_NOT_FOUND 0xC0000139)"
elif [ -z "$windows" ] && [ "$rc" -eq 127 ] && grep -q 'undefined symbol: polycall_' "$log"; then
    ok "1.0 library: loader refuses (exit 127): $(first 'undefined symbol')"
else
    bad "1.0 library: exit $rc (status 0x$nt) without the expected loader failure" "$log"
fi

echo "SUMMARY pass=$pass fail=$fail skip=$skip"
[ "$fail" -eq 0 ] || exit 1
[ "$skip" -eq 0 ] || exit 77
exit 0
