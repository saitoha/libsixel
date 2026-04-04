#!/bin/sh
# TAP test: builtin GIF force-loop exits promptly on SIGINT.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${SIXEL_TEST_C_COMPILER_ID-}" = "msvc" && {
    printf "1..0 # SKIP SIGINT child cancellation is unreliable on msvc runtime\n"
    exit 0
}

test "${SIXEL_TEST_C_COMPILER_ID-}" = "clang-cl" && {
    printf "1..0 # SKIP SIGINT child cancellation is unreliable on clang-cl runtime\n"
    exit 0
}


echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce "${input_gif}" \
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
    echo "not ok" 1 - "builtin GIF force-loop did not stop after SIGINT"
    exit 0
}

wait "${pid}" 2>/dev/null || true

echo "ok" 1 - "builtin GIF force-loop stops quickly on SIGINT"
exit 0
