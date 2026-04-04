#!/bin/sh
# Emit TAP for shellcheck static check.

set -eu

shellcheck_driver=$1
src_root=$2
shellcheck_bin=$3
output_mode=${SIXEL_STATICCHECK_MODE:-tap}

emit_plan() {
    if test "$output_mode" = "tap"; then
        echo "1..1"
    fi
}

emit_skip() {
    reason=$1
    if test "$output_mode" = "tap"; then
        echo "1..0 # SKIP $reason"
    else
        echo "SKIP: $reason"
    fi
}

emit_pass() {
    if test "$output_mode" = "tap"; then
        echo "ok 1 - shellcheck"
    else
        echo "PASS: shellcheck"
    fi
}

emit_fail() {
    if test "$output_mode" = "tap"; then
        echo "not ok 1 - shellcheck"
    else
        echo "FAIL: shellcheck"
    fi
}

if test -z "$shellcheck_bin"; then
    emit_skip "shellcheck not found"
    exit 0
fi

if test ! -x "$shellcheck_bin" && ! command -v "$shellcheck_bin" >/dev/null 2>&1; then
    emit_skip "shellcheck executable not found: $shellcheck_bin"
    exit 0
fi

emit_plan

if SHELLCHECK_CMD="$shellcheck_bin" "$shellcheck_driver" "$src_root"; then
    emit_pass
else
    emit_fail
    exit 1
fi
