#!/bin/sh
# Shared TAP helpers for shell-based tests.
#
# This module standardizes TAP output to the following format:
# - Plan:   "1..N"
# - Pass:   "ok <case> - <description>"
# - Fail:   "not ok <case> - <description>"
# - Skip:   "1..0 # SKIP <reason>"
#
# The helpers expose both tap_* names and pass/fail wrappers so existing
# scripts can opt into the shared implementation without rewriting every
# call site. When pass/fail are invoked with a single argument, the
# helpers assume the case number is 1 and treat the argument as the
# description to match historical single-case tests.

tap_status=0

tap_plan() {
    printf '1..%s\n' "$1"
}

tap_pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

tap_fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
    if [ -n "${tap_log_file:-}" ] && [ -f "${tap_log_file}" ]; then
        printf '# python log follows\n'
        sed 's/^/# /' "${tap_log_file}"
    fi
    tap_status=1
    status=1
}

tap_skip_all() {
    printf '1..0 # SKIP %s\n' "$1"
    exit 0
}

tap_skip() {
    printf 'ok %s # SKIP %s\n' "$1" "$2"
}

skip_all() {
    tap_skip_all "$1"
}

pass() {
    case $# in
        1) tap_pass 1 "$1" ;;
        2) tap_pass "$1" "$2" ;;
        *) tap_fail 1 "invalid pass() usage";;
    esac
}

fail() {
    case $# in
        1) tap_fail 1 "$1" ;;
        2) tap_fail "$1" "$2" ;;
        *) tap_fail 1 "invalid fail() usage";;
    esac
}
