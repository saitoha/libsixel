#!/bin/sh
# TAP test: loop=force ignores NETSCAPE loop2 and stays unbounded.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_loop2="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop2.gif"

# Launch directly so $! points to the actual wrapper process.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -lforce -g "${input_loop2}" \
    -o /dev/null >/dev/null 2>&1 &
pid=$!

wait_limit=3
while test "${wait_limit}" -gt 0; do
    kill -0 "${pid}" 2>/dev/null || {
        wait "${pid}" 2>/dev/null || true
        echo "not ok" 1 - "loop force ignores NETSCAPE loop2 unexpectedly finished"
        exit 0
    }
    sleep 1
    wait_limit=$((wait_limit - 1))
done

kill "${pid}" 2>/dev/null || true
wait "${pid}" 2>/dev/null || true

echo "ok" 1 - "loop force ignores NETSCAPE loop2"

exit 0
