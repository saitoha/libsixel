#!/bin/sh
# Simple timeout wrapper for TAP tests with fallbacks for macOS.

set -u

usage() {
    echo "Usage: $0 --timeout=SECONDS command [args ...]" >&2
    exit 64
}

timeout_seconds=""
while [ $# -gt 0 ]; do
    case "$1" in
        --timeout=*)
            timeout_seconds=${1#--timeout=}
            shift
            ;;
        --timeout)
            shift
            [ $# -gt 0 ] || usage
            timeout_seconds=$1
            shift
            ;;
        --help|-h)
            usage
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
    esac
    [ -n "$timeout_seconds" ] && break
done

[ -n "$timeout_seconds" ] || usage
case "$timeout_seconds" in
    *[!0-9]*)
        usage
        ;;
    "")
        usage
        ;;
    *)
        :
        ;;
esac

[ $# -gt 0 ] || usage

select_timeout_bin() {
    if command -v timeout >/dev/null 2>&1; then
        echo "timeout"
        return
    fi
    if command -v gtimeout >/dev/null 2>&1; then
        echo "gtimeout"
        return
    fi
    echo ""
}

run_with_helper() {
    helper=$1
    shift
    "$helper" -k 10s "${timeout_seconds}s" "$@"
    status=$?
    if [ "$status" -eq 124 ] || [ "$status" -eq 137 ]; then
        echo "Bail out! timeout after ${timeout_seconds}s" >&2
    fi
    exit "$status"
}

run_with_watchdog() {
    flag_file=$(mktemp -t with-timeout.XXXXXX 2>/dev/null || mktemp /tmp/with-timeout.XXXXXX)
    trap 'rm -f "$flag_file"' EXIT

    "$@" &
    cmd_pid=$!

    (
        sleep "$timeout_seconds"
        echo "expired" > "$flag_file"
        kill -TERM "$cmd_pid" 2>/dev/null || exit 0
        sleep 10
        kill -KILL "$cmd_pid" 2>/dev/null || true
    ) &
    watchdog_pid=$!

    wait "$cmd_pid"
    cmd_status=$?

    if [ -f "$flag_file" ]; then
        rm -f "$flag_file"
        kill "$watchdog_pid" 2>/dev/null || true
        echo "Bail out! timeout after ${timeout_seconds}s" >&2
        exit 124
    fi

    kill "$watchdog_pid" 2>/dev/null || true
    rm -f "$flag_file"
    exit "$cmd_status"
}

helper_bin=$(select_timeout_bin)

# Allow running non-executable test scripts (e.g., when permission bits are
# stripped in certain environments) by prepending the current shell.
if [ $# -gt 0 ] && [ -f "$1" ] && [ ! -x "$1" ]; then
    set -- "$SHELL" "$@"
fi

if [ -n "$helper_bin" ]; then
    run_with_helper "$helper_bin" "$@"
fi

run_with_watchdog "$@"
