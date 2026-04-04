#!/bin/sh
# TAP test: libwebp pipeline emits cancel stop trace on SIGINT.

set -eux

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

test "${HAVE_WINDOWS_H-0}" = 1 && {
    printf "1..0 # SKIP signal trace check is unavailable on Windows\n"
    exit 0
}

echo "1..1"
set -v

input_webp="${TOP_SRCDIR}/tests/data/inputs/formats/animated-lossless-8x8-2frame-loop2-min.webp"

trace_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env "SIXEL_THREADS=4" \
        --env "SIXEL_TRACE_TOPIC=encode_handoff" \
        -Llibwebp! -lforce "${input_webp}" 2>&1 >/dev/null &
    pid=$!

    sleep 1
    kill -INT "${pid}" 2>/dev/null || true

    wait_limit=40
    while test "${wait_limit}" -gt 0; do
        kill -0 "${pid}" 2>/dev/null || {
            break
        }
        sleep 0.05
        wait_limit=$((wait_limit - 1))
    done

    kill -0 "${pid}" 2>/dev/null && {
        kill -KILL "${pid}" 2>/dev/null || true
        wait "${pid}" 2>/dev/null || true
        printf "__TIMEOUT__\n"
    }

    wait "${pid}" 2>/dev/null || true
)

test "${trace_output#*"__TIMEOUT__"}" = "${trace_output}" || {
    echo "not ok" 1 - "libwebp pipeline did not stop after SIGINT"
    exit 0
}

test "${trace_output#*"event=callback_handoff_decide handoff=pipeline"}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "libwebp pipeline handoff trace missing"
    exit 0
}

test "${trace_output#*"event=pipeline_stop"}" != "${trace_output}" || {
    echo "not ok" 1 - "libwebp pipeline stop reason trace missing"
    exit 0
}

echo "ok" 1 - "libwebp pipeline emits cancel stop trace on SIGINT"
exit 0
