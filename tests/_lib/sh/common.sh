#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

runtime_exec() {
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
