#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

runtime_exec() {
    runtime_libdir="${TOP_BUILDDIR}/src/.libs"
    runtime_var="${RUNTIME_SHLIBPATH_VAR:-LD_LIBRARY_PATH}"
    runtime_sep="${RUNTIME_SHLIBPATH_SEP:-:}"
    runtime_current=""
    runtime_value=""
    runtime_extra="${SIXEL_TEST_ADDITIOANL_PATH:-}"
    shlibpath_overrides_runpath="${SIXEL_SHLIBPATH_OVERRIDES_RUNPATH:-yes}"

    if [ "${shlibpath_overrides_runpath}" = "yes" ]; then
        eval "runtime_current=\${${runtime_var}:-}"

        runtime_value="${runtime_libdir}"
        if [ -n "${runtime_extra}" ]; then
            runtime_value="${runtime_value}${runtime_sep}${runtime_extra}"
        fi
        if [ -n "${runtime_current}" ]; then
            runtime_value="${runtime_value}${runtime_sep}${runtime_current}"
        fi

        eval "${runtime_var}=\${runtime_value}"
        eval "export ${runtime_var}"
    fi

    if [ -n "${SIXEL_RUNTIME-}" ]; then
        "${SIXEL_RUNTIME-}" "$@"
    else
        "$@"
    fi
}

run_img2sixel() {
    runtime_exec "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    runtime_exec "${SIXEL2PNG_PATH}" "$@"
}

run_lsqa() {
    runtime_exec "${LSQA_PATH}" "$@"
}

run_test_runner() {
    runtime_exec "${TEST_RUNNER_PATH}" "$@"
}

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

pass() {
    printf 'ok %s - %s\n' "$1" "$2"
}

fail() {
    printf 'not ok %s - %s\n' "$1" "$2"
}
