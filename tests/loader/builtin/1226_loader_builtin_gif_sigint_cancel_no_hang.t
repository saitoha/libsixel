#!/bin/sh
# TAP test: builtin GIF force-loop exits promptly on SIGINT.

set -eux

: "${TEST_RUNNER_PATH:=${TOP_BUILDDIR}/tests/test_runner${SIXEL_BIN_EXT-}}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_EMSCRIPTEN_H-0}" = 1 && {
    printf "1..0 # SKIP emscripten test_runner lacks --sigint-run support\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"
ctrl_break_mode=0
build_os="${RUNTIME_ENV_BUILD_OS-unknown}"
build_os_is_darwin=0
sigint_runner_mode=0
test "${build_os}" != "${build_os#darwin}" && build_os_is_darwin=1

# Wine on macOS does not reliably forward host SIGINT to wrapped
# Windows processes. Skip this SIGINT-specific assertion to avoid
# reporting platform signal-delivery behavior as a loader regression.
test "${HAVE_WINDOWS_H-0}" = 1 && \
    test "${RUNTIME_ENV_IS_WINE-0}" = 1 && \
    test "${build_os_is_darwin}" = 1 && {
    echo "ok 1 - builtin GIF SIGINT cancellation # SKIP wine/macOS signal forwarding is unreliable"
    exit 0
}

test "${HAVE_WINDOWS_H-0}" = 1 && ctrl_break_mode=1
test -x "${TEST_RUNNER_PATH-}" || ctrl_break_mode=0
test -n "${SIXEL_RUNTIME-}" && ctrl_break_mode=0

test "${HAVE_WINDOWS_H-0}" = 0 && sigint_runner_mode=1
test -x "${TEST_RUNNER_PATH-}" || sigint_runner_mode=0
test -n "${SIXEL_RUNTIME-}" && sigint_runner_mode=0

test "${ctrl_break_mode}" = "1" && {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --win32-ctrl-break-run \
        0 0 "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce "${input_gif}" \
        >/dev/null || {
        echo "not ok" 1 - "builtin GIF force-loop did not stop after CTRL_BREAK"
        exit 0
    }
    echo "ok" 1 - "builtin GIF force-loop stops on CTRL_BREAK"
    exit 0
}

test "${sigint_runner_mode}" = "1" && {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --sigint-run \
        0 0 "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce -g "${input_gif}" \
        >/dev/null || {
        echo "not ok" 1 - "builtin GIF force-loop did not stop after SIGINT"
        exit 0
    }
    echo "ok" 1 - "builtin GIF force-loop stops on SIGINT"
    exit 0
}

echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP no signal-capable test_runner path in this runtime"
exit 0
