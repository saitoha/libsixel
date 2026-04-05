#!/bin/sh
# TAP test: builtin GIF force-loop exits promptly on SIGINT.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"
ctrl_break_ok=0
ctrl_break_mode=0
platform_name=$(uname -s 2>/dev/null || printf "unknown")
platform_not_cygwin="${platform_name#CYGWIN}"

test "${HAVE_WINDOWS_H-0}" = 1 && ctrl_break_mode=1
test -x "${TEST_RUNNER_PATH-}" || ctrl_break_mode=0
test "${platform_not_cygwin}" != "${platform_name}" && ctrl_break_mode=0
test -n "${SIXEL_RUNTIME-}" && ctrl_break_mode=0

test "${ctrl_break_mode}" = "1" && {
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" --win32-ctrl-break-run \
        1000 2000 "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce "${input_gif}" \
        >/dev/null && ctrl_break_ok=1
    test "${ctrl_break_ok}" = "1" && {
        echo "ok" 1 - "builtin GIF force-loop stops quickly on CTRL_BREAK"
        exit 0
    }
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce "${input_gif}" \
    >/dev/null 2>/dev/null &
pid=$!

sleep 1
kill -INT "${pid}" 2>/dev/null || true

wait_limit=40
# Runtime wrappers such as wine can delay SIGINT delivery to the target.
# Keep native runs strict while allowing extra grace time for wrapped runs.
test -n "${SIXEL_RUNTIME-}" && wait_limit=200
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
