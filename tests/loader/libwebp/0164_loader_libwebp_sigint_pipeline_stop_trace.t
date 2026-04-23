#!/bin/sh
# TAP test: libwebp pipeline emits cancel stop trace on SIGINT.

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

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread support is disabled\n"
    exit 0
}

test "${HAVE_EMSCRIPTEN_H-0}" = 1 && {
    printf "1..0 # SKIP emscripten runtime may exit before stop trace\n"
    exit 0
}

test "${HAVE_WINDOWS_H-0}" = 1 && {
    printf "1..0 # SKIP signal trace check is unavailable on Windows\n"
    exit 0
}

test "${SIXEL_TSAN_BUILD-no}" = "yes" && test "${HAVE_APPKIT-0}" = 1 && {
    printf "1..0 # SKIP macOS TSan can stall SIGINT pipeline trace timing\n"
    exit 0
}

build_os="${RUNTIME_ENV_BUILD_OS-unknown}"
test "${build_os}" != "${build_os#haiku}" && {
    printf "1..0 # SKIP Haiku signal timing is unstable for this trace test\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"
#
# Drive SIGINT from the trace token itself, not from a wall-clock budget.
# A zero timeout disables the runner-side watchdog and avoids timing tuning.
#

set +xv
sigint_run_status=0
trace_summary=$(
    # shellcheck disable=SC2086
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
        --sigint-run-until \
        "event=callback_handoff_decide" \
        0 \
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env "SIXEL_THREADS=4" \
        --env "SIXEL_TRACE_TOPIC=encode_handoff" \
        --env "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=1" \
        -Llibwebp! -lforce -o /dev/null -g "${input_webp}" 2>&1 >/dev/null
) || sigint_run_status=$?

handoff_pipeline_flag=0
handoff_serial_flag=0
pipeline_stop_flag=0

trace_remainder=${trace_summary#*event=callback_handoff_decide handoff=pipeline}
test "${trace_remainder}" != "${trace_summary}" && handoff_pipeline_flag=1

trace_remainder=${trace_summary#*event=callback_handoff_decide handoff=serial}
test "${trace_remainder}" != "${trace_summary}" && handoff_serial_flag=1

trace_remainder=${trace_summary#*event=pipeline_stop handoff=pipeline}
test "${trace_remainder}" != "${trace_summary}" && pipeline_stop_flag=1
set -xv

test "${sigint_run_status}" = "0" || {
    printf '%s\n' "${trace_summary}"
    echo "not ok" 1 - "libwebp pipeline did not stop after SIGINT"
    exit 0
}

test "${handoff_pipeline_flag}" = "1" || {
    test "${handoff_serial_flag}" = "1" && {
        echo "ok 1 - libwebp pipeline stop trace # SKIP runtime selected serial handoff"
        exit 0
    }
    printf '%s\n' "${trace_summary}"
    echo "not ok" 1 - "libwebp pipeline handoff trace missing"
    exit 0
}

test "${pipeline_stop_flag}" = "1" || {
    printf '%s\n' "${trace_summary}"
    echo "not ok" 1 - "libwebp pipeline stop reason trace missing"
    exit 0
}

echo "ok" 1 - "libwebp pipeline emits cancel stop trace on SIGINT"
exit 0
