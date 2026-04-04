#!/bin/sh
# TAP test: libwebp animation force-loop exits promptly on SIGINT.

set -eux

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

test "${HAVE_WINDOWS_H-0}" = 1 && {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --win32-ctrl-break-run \
        1000 2000 "${IMG2SIXEL_PATH}" -Llibwebp! -lforce "${input_webp}" \
        >/dev/null || {
        echo "not ok" 1 - "libwebp force-loop did not stop after CTRL_BREAK"
        exit 0
    }
    echo "ok" 1 - "libwebp force-loop stops quickly on CTRL_BREAK"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Llibwebp! -lforce "${input_webp}" \
    >/dev/null 2>/dev/null &
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
    echo "not ok" 1 - "libwebp force-loop did not stop after SIGINT"
    exit 0
}

wait "${pid}" 2>/dev/null || true

echo "ok" 1 - "libwebp force-loop stops quickly on SIGINT"
exit 0
