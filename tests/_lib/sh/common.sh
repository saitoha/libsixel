#!/bin/sh
# Common helpers for converter TAP tests executed with POSIX sh.

run_img2sixel() {
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" "$@"
}

run_sixel2png() {
    ${SIXEL_RUNTIME-} "${SIXEL2PNG_PATH}" "$@"
}

run_lsqa() {
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" "$@"
}

run_test_runner() {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "$@"
}
