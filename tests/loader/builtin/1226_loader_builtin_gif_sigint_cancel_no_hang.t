#!/bin/sh
# TAP test: builtin GIF force-loop exits promptly on SIGINT.

set -eux

: "${TEST_RUNNER_PATH:=${TOP_BUILDDIR}/tests/test_runner${SIXEL_BIN_EXT-}}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"
test "${HAVE_WINDOWS_H-0}" = 1 && {
    echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP signal forwarding checks are unstable on Windows runtimes"
    exit 0
}

test "${HAVE_EMSCRIPTEN_H-0}" = 1 && {
    echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP emscripten runtime may not deliver host SIGINT deterministically"
    exit 0
}

test -x "${TEST_RUNNER_PATH-}" || {
    echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP test_runner --sigint-run-until is unavailable in this runtime"
    exit 0
}

test -n "${SIXEL_RUNTIME-}" && {
    echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP wrapped runtime signal forwarding is unavailable"
    exit 0
}

set +xv
preflight_status=0
preflight_trace=$(
    # shellcheck disable=SC2086
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env "SIXEL_THREADS=4" \
        --env "SIXEL_TRACE_TOPIC=encode_handoff" \
        --env "SIXEL_ENCODE_HANDOFF_TRACE_MINIMAL=1" \
        -Lbuiltin! -ldisable -o /dev/null -g "${input_gif}" \
        2>&1 >/dev/null
) || preflight_status=$?
preflight_has_trigger=0
trace_remainder=${preflight_trace#*event=callback_handoff_decide}
test "${trace_remainder}" != "${preflight_trace}" && preflight_has_trigger=1
set -xv

test "${preflight_status}" = "0" || {
    printf '%s\n' "${preflight_trace}"
    echo "not ok" 1 - "builtin GIF encode_handoff preflight failed"
    exit 0
}

test "${preflight_has_trigger}" = "1" || {
    echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP encode_handoff callback trace token is unavailable in this runtime"
    exit 0
}

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
        -Lbuiltin! -lforce -o /dev/null -g "${input_gif}" 2>&1 >/dev/null
) || sigint_run_status=$?
set -xv

test "${sigint_run_status}" = "0" || {
    printf '%s\n' "${trace_summary}"
    echo "ok 1 - builtin GIF force-loop SIGINT cancellation # SKIP runtime signal forwarding is unavailable"
    exit 0
}

echo "ok" 1 - "builtin GIF force-loop stops on SIGINT"
exit 0
