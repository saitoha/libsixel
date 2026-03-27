#!/bin/sh
# TAP test: disposal-heavy GIF decode completes within watchdog window.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


printf '1..1\n'
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-disposal-stress-anim.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -ldisable -g \
              "${input_gif}" -o /dev/null >/dev/null 2>&1 &
pid=$!

wait_limit=10
while test "${wait_limit}" -gt 0; do
    kill -0 "${pid}" 2>/dev/null || {
        break
    }
    sleep 1
    wait_limit=$((wait_limit - 1))
done

kill -0 "${pid}" 2>/dev/null && {
    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
    echo "not ok" 1 - "disposal stress decode exceeded watchdog"
    exit 0
}

wait "${pid}" || rc=$?
test "${rc-0}" -eq 0 || {
    echo "not ok" 1 - "disposal stress decode failed"
    exit 0
}

echo "ok" 1 - "disposal stress decode completed within watchdog"
exit 0
