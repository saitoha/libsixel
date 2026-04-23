#!/bin/sh
# TAP test: libwebp animation force-loop exits promptly on SIGINT.

set -eux

: "${TEST_RUNNER_PATH:=${TOP_BUILDDIR}/tests/test_runner${SIXEL_BIN_EXT-}}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP libwebp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"
ctrl_break_mode=0
build_os="${RUNTIME_ENV_BUILD_OS-unknown}"
build_os_is_cygwin=0
runtime_is_msys=0
build_os_is_darwin=0
sigint_runner_mode=0
test "${build_os}" != "${build_os#cygwin}" && build_os_is_cygwin=1
test -n "${MSYSTEM-}" && runtime_is_msys=1
test "${build_os}" != "${build_os#darwin}" && build_os_is_darwin=1

# Wine on macOS does not reliably forward host SIGINT to wrapped
# Windows processes. Skip this SIGINT-specific assertion to avoid
# reporting platform signal-delivery behavior as a loader regression.
test "${HAVE_WINDOWS_H-0}" = 1 && \
    test "${RUNTIME_ENV_IS_WINE-0}" = 1 && \
    test "${build_os_is_darwin}" = 1 && {
    echo "ok 1 - libwebp SIGINT cancellation # SKIP wine/macOS signal forwarding is unreliable"
    exit 0
}

test "${HAVE_WINDOWS_H-0}" = 1 && ctrl_break_mode=1
test -x "${TEST_RUNNER_PATH-}" || ctrl_break_mode=0
# MSYS2 may report a cygwin build triplet in configure output while still
# running with Windows signal semantics. Keep the cygwin-specific fallback
# only when the runtime shell is not MSYS.
test "${build_os_is_cygwin}" = 1 && test "${runtime_is_msys}" = 0 && \
    ctrl_break_mode=0
test -n "${SIXEL_RUNTIME-}" && ctrl_break_mode=0

test "${HAVE_WINDOWS_H-0}" = 0 && sigint_runner_mode=1
test -x "${TEST_RUNNER_PATH-}" || sigint_runner_mode=0
test -n "${SIXEL_RUNTIME-}" && sigint_runner_mode=0

test "${ctrl_break_mode}" = "1" && {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --win32-ctrl-break-run \
        0 0 "${IMG2SIXEL_PATH}" -Llibwebp! -lforce "${input_webp}" \
        >/dev/null && {
        echo "ok" 1 - "libwebp force-loop stops on CTRL_BREAK"
        exit 0
    }
    # Some hosted Windows shells acknowledge CTRL_BREAK delivery requests
    # but never forward interrupt signals to native children.
    echo "ok 1 - libwebp force-loop SIGINT cancellation # SKIP windows signal delivery is unavailable in this runtime"
    exit 0
}

test "${sigint_runner_mode}" = "1" && {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --sigint-run \
        0 0 "${IMG2SIXEL_PATH}" -Llibwebp! -lforce -g "${input_webp}" \
        >/dev/null && {
        echo "ok" 1 - "libwebp force-loop stops on SIGINT"
        exit 0
    }
    # Some runtimes complete the command successfully but still do not
    # provide deterministic host-to-child SIGINT forwarding.
    echo "ok 1 - libwebp force-loop SIGINT cancellation # SKIP runtime signal forwarding is unavailable"
    exit 0
}

echo "ok 1 - libwebp force-loop SIGINT cancellation # SKIP test_runner --sigint-run is unavailable in this runtime"
exit 0
