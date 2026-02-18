#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

runtime_exec() {
    runtime_libdir="${TOP_BUILDDIR}/src/.libs"
    runtime_var="${RUNTIME_SHLIBPATH_VAR:-LD_LIBRARY_PATH}"
    runtime_sep="${RUNTIME_SHLIBPATH_SEP:-:}"
    runtime_current=""
    runtime_value=""
    shlibpath_overrides_runpath="${SIXEL_SHLIBPATH_OVERRIDES_RUNPATH:-yes}"

    if [ "${shlibpath_overrides_runpath}" = "yes" ]; then
        eval "runtime_current=\${${runtime_var}:-}"

        if [ -n "${runtime_current}" ]; then
            runtime_value="${runtime_libdir}${runtime_sep}${runtime_current}"
        else
            # shellcheck disable=SC2034
            runtime_value="${runtime_libdir}"
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

# Execute a helper command and optionally export extra environment values
# for that specific invocation.
#
# Usage:
#   run_img2sixel --env KEY=VALUE,KEY2=VALUE2 [--] <args...>
#   run_sixel2png --env KEY=VALUE [--] <args...>
#   run_lsqa --env KEY=VALUE [--] <args...>
#   run_test_runner --env KEY=VALUE [--] <args...>
#
# The --env option can be specified multiple times and accepts a comma-separated
# list. This keeps behavior consistent even when SIXEL_RUNTIME wraps the target
# command (for example node or wine), because the variables are exported before
# runtime_exec invokes the wrapper.
run_with_optional_env() {
    tool_path=$1
    env_items=""
    env_chunk=""
    old_ifs=""

    shift
    while [ $# -gt 0 ]; do
        case "$1" in
        --env)
            if [ $# -lt 2 ]; then
                printf '%s\n' "run helper: --env requires KEY=VALUE list" >&2
                return 2
            fi
            if [ -n "${env_items}" ]; then
                env_items="${env_items},$2"
            else
                env_items="$2"
            fi
            shift 2
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
        esac
    done

    if [ -z "${env_items}" ]; then
        runtime_exec "${tool_path}" "$@"
        return $?
    fi

    old_ifs=$IFS
    IFS=,
    for env_chunk in ${env_items}; do
        case "${env_chunk}" in
        *=*)
            # shellcheck disable=SC2163
            export "${env_chunk}"
            ;;
        *)
            printf '%s\n' "run helper: invalid env assignment: ${env_chunk}" >&2
            exit 2
            ;;
        esac
    done
    IFS=${old_ifs}
    runtime_exec "${tool_path}" "$@"
}

run_img2sixel() {
    run_with_optional_env "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    run_with_optional_env "${SIXEL2PNG_PATH}" "$@"
}

run_lsqa() {
    run_with_optional_env "${LSQA_PATH}" "$@"
}

run_test_runner() {
    run_with_optional_env "${TEST_RUNNER_PATH}" "$@"
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
