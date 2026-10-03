#!/usr/bin/env sh
# Run a binding test program against the REAL Polycall core with live C-CLI
# counterparts:
#
#   tests/run-real.sh <test-command> [args...]
#
# Starts, on ephemeral loopback ports with a private state directory,
#   - `polycall peer serve` (node id cli-node),
#   - `polycall start` (foreground RPC runtime),
#   - `polycall daemon start` (background daemon for a scratch Polycallfile),
# exports
#   POLYCALL_CLI, POLYCALL_CLI_PEER, POLYCALL_RPC_ENDPOINT,
#   POLYCALL_DAEMON_ENDPOINT, POLYCALL_DEV_TOKEN
# and runs the command. Exit 77 (SKIP) when the polycall CLI is not
# available -- a missing toolchain is never reported as success.
# Works under POSIX sh on Linux and under Git Bash / MSYS2 on Windows.
set -eu

if [ "$#" -eq 0 ]; then
    echo "usage: $0 <test-command> [args...]" >&2
    exit 2
fi

cli=${POLYCALL_CLI:-}
if [ -z "$cli" ]; then
    cli=$(command -v polycall 2>/dev/null || command -v polycall.exe 2>/dev/null || true)
fi
if [ -z "$cli" ] || [ ! -f "$cli" ]; then
    echo "SKIP: polycall CLI not found (set POLYCALL_CLI or put polycall on PATH)"
    exit 77
fi

native() {
    if command -v cygpath >/dev/null 2>&1; then cygpath -w "$1"; else printf '%s\n' "$1"; fi
}
export POLYCALL_CLI="$(native "$cli")"
if [ -z "${POLYCALL_DEV_TOKEN:-}" ]; then
    POLYCALL_DEV_TOKEN="qa-$(od -An -N12 -tx1 /dev/urandom | tr -d ' \n')"
fi
export POLYCALL_DEV_TOKEN
export POLYCALL_TELEMETRY=off

work="$(pwd)/.polycall-qa.$$"
rm -rf "$work"
mkdir "$work"
serve_pid=""
rpc_pid=""
rpc_ep=""
daemon_started=""
printf 'log_level=info\n' >"$work/Polycallfile"
daemon_dir=$(native "$work/daemon")
daemon_file=$(native "$work/Polycallfile")

cleanup() {
    if [ -n "$daemon_started" ]; then
        "$cli" daemon stop --force --state-dir "$daemon_dir" "$daemon_file" >/dev/null 2>&1 || true
    fi
    if [ -n "$rpc_ep" ]; then
        "$cli" stop --endpoint "$rpc_ep" --auth-token "$POLYCALL_DEV_TOKEN" >/dev/null 2>&1 || true
    fi
    [ -n "$serve_pid" ] && kill "$serve_pid" 2>/dev/null || true
    [ -n "$rpc_pid" ] && kill "$rpc_pid" 2>/dev/null || true
    [ -n "$serve_pid" ] && wait "$serve_pid" 2>/dev/null || true
    [ -n "$rpc_pid" ] && wait "$rpc_pid" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT
trap 'exit 130' INT TERM

"$cli" peer serve --node-id cli-node --endpoint 127.0.0.1:0 \
    --endpoint-file "$(native "$work/peer.ep")" >"$work/serve.log" 2>&1 &
serve_pid=$!
"$cli" start --endpoint 127.0.0.1:0 --endpoint-file "$(native "$work/rpc.ep")" \
    --auth-token "$POLYCALL_DEV_TOKEN" >"$work/rpc.log" 2>&1 &
rpc_pid=$!
# `daemon start` returns once the detached daemon answers health.
if ! "$cli" daemon start --endpoint 127.0.0.1:0 --state-dir "$daemon_dir" "$daemon_file" >"$work/daemon-start.log" 2>&1; then
    echo "FAIL: polycall daemon start" >&2
    cat "$work/daemon-start.log" >&2 || true
    exit 1
fi
daemon_started=1

i=0
while [ ! -s "$work/peer.ep" ] || [ ! -s "$work/rpc.ep" ]; do
    i=$((i + 1))
    if [ "$i" -gt 100 ]; then
        echo "FAIL: polycall peer serve / polycall start did not come up" >&2
        cat "$work/serve.log" "$work/rpc.log" >&2 || true
        exit 1
    fi
    sleep 0.1
done
POLYCALL_CLI_PEER=$(tr -d '\r\n' <"$work/peer.ep")
rpc_ep=$(tr -d '\r\n' <"$work/rpc.ep")
POLYCALL_RPC_ENDPOINT=$rpc_ep
POLYCALL_DAEMON_ENDPOINT=$(tr -d '\r\n' <"$work/daemon/daemon.json" | sed -n 's/.*"endpoint":"\([^"]*\)".*/\1/p')
if [ -z "$POLYCALL_DAEMON_ENDPOINT" ]; then
    echo "FAIL: polycall daemon wrote no endpoint to daemon.json" >&2
    exit 1
fi
export POLYCALL_CLI_PEER POLYCALL_RPC_ENDPOINT POLYCALL_DAEMON_ENDPOINT
echo "run-real: polycall CLI $("$cli" --version | tr -d '\r'), peer node $POLYCALL_CLI_PEER, runtime $POLYCALL_RPC_ENDPOINT, daemon $POLYCALL_DAEMON_ENDPOINT"

set +e
"$@"
status=$?
set -e
exit "$status"
