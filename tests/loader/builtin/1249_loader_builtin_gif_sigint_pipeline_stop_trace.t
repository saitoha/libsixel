#!/bin/sh
# TAP test: builtin GIF pipeline emits cancel stop trace on SIGINT.

set -eux

: "${TEST_RUNNER_PATH:=${TOP_BUILDDIR}/tests/test_runner${SIXEL_BIN_EXT-}}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
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

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"
# Keep the common case fast with an early SIGINT.
# If the first run misses trace coverage, retry once with a longer window.

set +xv
sigint_run_status=0
trace_summary=$(
    # shellcheck disable=SC2086
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --sigint-run 12 300 \
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env "SIXEL_THREADS=4" \
        --env "SIXEL_TRACE_TOPIC=encode_handoff" \
        --env "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=1" \
        -Lbuiltin! -lforce -g "${input_gif}" 2>&1 >/dev/null
) || sigint_run_status=$?

handoff_flag=0
stop_flag=0
retry_flag=0

trace_remainder=${trace_summary#*event=callback_handoff_decide handoff=pipeline}
test "${trace_remainder}" != "${trace_summary}" && handoff_flag=1

trace_remainder=${trace_summary#*event=pipeline_stop}
test "${trace_remainder}" != "${trace_summary}" && stop_flag=1

test "${sigint_run_status}" = "0" || retry_flag=1
test "${handoff_flag}" = "1" || retry_flag=1
test "${stop_flag}" = "1" || retry_flag=1

test "${retry_flag}" = "0" || {
    set +xv
    sigint_run_status=0
    trace_summary=$(
        # shellcheck disable=SC2086
        ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --sigint-run 80 300 \
            ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
            --env "SIXEL_THREADS=4" \
            --env "SIXEL_TRACE_TOPIC=encode_handoff" \
            --env "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=1" \
            -Lbuiltin! -lforce -g "${input_gif}" 2>&1 >/dev/null
    ) || sigint_run_status=$?

    handoff_flag=0
    stop_flag=0

    trace_remainder=${trace_summary#*event=callback_handoff_decide handoff=pipeline}
    test "${trace_remainder}" != "${trace_summary}" && handoff_flag=1

    trace_remainder=${trace_summary#*event=pipeline_stop}
    test "${trace_remainder}" != "${trace_summary}" && stop_flag=1
}
set -xv

test "${sigint_run_status}" = "0" || {
    echo "not ok" 1 - "builtin GIF pipeline did not stop after SIGINT"
    exit 0
}

test "${handoff_flag}" = "1" || {
    echo "not ok" 1 - "builtin GIF pipeline handoff trace missing"
    exit 0
}

test "${stop_flag}" = "1" || {
    echo "not ok" 1 - "builtin GIF pipeline stop reason trace missing"
    exit 0
}

echo "ok" 1 - "builtin GIF pipeline emits cancel stop trace on SIGINT"
exit 0
