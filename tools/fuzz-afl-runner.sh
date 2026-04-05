#!/usr/bin/env bash
#
# Shared AFL++ launcher used by both Autotools and Meson fuzz targets.
#
# This wrapper centralizes multi-worker orchestration, timeout handling, and
# optional CmpLog execution so CI can control fuzz depth through environment
# variables without duplicating shell logic in multiple build files.

set -euo pipefail

timeout() {
    local timeout_duration
    local cmd_pid
    local watchdog_pid
    local cmd_status

    if [ "$#" -lt 2 ]; then
        echo "usage: timeout SECONDS command [args...]" >&2
        return 125
    fi

    timeout_duration="$1"
    shift

    (
        if command -v setsid >/dev/null 2>&1; then
            setsid "$@" &
        else
            "$@" &
        fi

        cmd_pid=$!

        (
            sleep "$timeout_duration"
            kill -TERM -"$cmd_pid" 2>/dev/null
            sleep 1
            kill -KILL -"$cmd_pid" 2>/dev/null
        ) &
        watchdog_pid=$!

        set +e
        wait "$cmd_pid"
        cmd_status=$?
        set -e
        kill "$watchdog_pid" 2>/dev/null || true
        wait "$watchdog_pid" 2>/dev/null || true
        case "$cmd_status" in
            137|143)
                exit 124
                ;;
            *)
                exit "$cmd_status"
                ;;
        esac
    )
    return $?
}

if [ "$#" -lt 7 ]; then
    cat >&2 <<'USAGE'
usage: fuzz-afl-runner.sh <label> <afl-fuzz> <input_dir> <output_dir> <timeout_sec> -- <target_command> [args...]
USAGE
    exit 2
fi

label="$1"
fuzz_afl="$2"
input_dir="$3"
output_dir="$4"
fuzz_timeout="$5"
shift 5

if [ "$1" != "--" ]; then
    echo "error: expected '--' before target command" >&2
    exit 2
fi
shift

if [ "$#" -lt 1 ]; then
    echo "error: target command is required" >&2
    exit 2
fi

if ! command -v "$fuzz_afl" >/dev/null 2>&1; then
    echo "error: $fuzz_afl is not installed" >&2
    exit 1
fi

if [ ! -d "$input_dir" ]; then
    echo "error: fuzz input directory was not found: $input_dir" >&2
    exit 1
fi

case "$fuzz_timeout" in
    ''|*[!0-9]*)
        echo "error: timeout must be a non-negative integer: $fuzz_timeout" >&2
        exit 1
        ;;
esac

workers="${FUZZ_AFL_WORKERS:-1}"
case "$workers" in
    ''|*[!0-9]*|0)
        echo "error: FUZZ_AFL_WORKERS must be >= 1: $workers" >&2
        exit 1
        ;;
esac

mode="${FUZZ_AFL_MODE:-normal}"
if [ "$mode" != "normal" ] && [ "$mode" != "cmplog" ]; then
    echo "error: FUZZ_AFL_MODE must be 'normal' or 'cmplog': $mode" >&2
    exit 1
fi

cmplog_runner="${FUZZ_AFL_CMPLOG_RUNNER:-}"
if [ "$mode" = "cmplog" ]; then
    if [ -z "$cmplog_runner" ]; then
        echo "error: FUZZ_AFL_CMPLOG_RUNNER is required when FUZZ_AFL_MODE=cmplog" >&2
        exit 1
    fi
    if [ -f "$cmplog_runner" ] && head -n 1 "$cmplog_runner" | grep -q '^#!'; then
        cmplog_candidate="$(dirname "$cmplog_runner")/.libs/$(basename "$cmplog_runner")"
        if [ -x "$cmplog_candidate" ]; then
            cmplog_runner="$cmplog_candidate"
        fi
    fi
    if [ ! -x "$cmplog_runner" ]; then
        echo "error: CmpLog runner is not executable: $cmplog_runner" >&2
        exit 1
    fi
fi

mkdir -p "$output_dir"

extra_opts=()
if [ -n "${FUZZ_AFL_EXTRA_OPTS:-}" ]; then
    # Intentionally split the option string for ergonomic env-var control.
    # shellcheck disable=SC2206
    extra_opts=(${FUZZ_AFL_EXTRA_OPTS})
fi

target_cmd=("$@")

run_worker() {
    worker_index="$1"
    afl_cmd=("$fuzz_afl" -i "$input_dir" -o "$output_dir")
    worker_name=""

    if [ "$workers" -gt 1 ]; then
        worker_name=$(printf 'fuzzer%02d' "$worker_index")
        if [ "$worker_index" -eq 1 ]; then
            afl_cmd+=(-M "$worker_name")
        else
            afl_cmd+=(-S "$worker_name")
        fi
    fi

    if [ "$mode" = "cmplog" ]; then
        afl_cmd+=(-c "$cmplog_runner")
    fi

    if [ "${#extra_opts[@]}" -gt 0 ]; then
        afl_cmd+=("${extra_opts[@]}")
    fi

    afl_cmd+=(-- "${target_cmd[@]}")

    if [ "$workers" -gt 1 ]; then
        echo "$label: launch worker=$worker_name mode=$mode"
    else
        echo "$label: launch mode=$mode"
    fi

    if [ "$fuzz_timeout" -gt 0 ]; then
        timeout "$fuzz_timeout" "${afl_cmd[@]}"
    else
        "${afl_cmd[@]}"
    fi
}

timed_out=0
fatal_rc=0

if [ "$workers" -eq 1 ]; then
    set +e
    run_worker 1
    rc=$?
    set -e
    if [ "$rc" -eq 124 ]; then
        timed_out=1
    elif [ "$rc" -ne 0 ]; then
        fatal_rc="$rc"
    fi
else
    pids=()
    worker_ids=()
    worker_id=1
    while [ "$worker_id" -le "$workers" ]; do
        run_worker "$worker_id" &
        pids+=("$!")
        worker_ids+=("$worker_id")
        worker_id=$((worker_id + 1))
    done

    set +e
    for idx in "${!pids[@]}"; do
        pid="${pids[$idx]}"
        wid="${worker_ids[$idx]}"
        wait "$pid"
        rc=$?
        if [ "$rc" -eq 124 ]; then
            timed_out=1
            continue
        fi
        if [ "$rc" -ne 0 ] && [ "$fatal_rc" -eq 0 ]; then
            fatal_rc="$rc"
            echo "$label: worker $(printf 'fuzzer%02d' "$wid") failed rc=$rc" >&2
            for k in "${!pids[@]}"; do
                if [ "$k" -eq "$idx" ]; then
                    continue
                fi
                kill "${pids[$k]}" >/dev/null 2>&1 || true
            done
        fi
    done
    set -e
fi

if [ "$fatal_rc" -ne 0 ]; then
    exit "$fatal_rc"
fi

if [ "$timed_out" -eq 1 ]; then
    echo "$label: reached FUZZ_TIMEOUT=${fuzz_timeout}s"
fi

exit 0
