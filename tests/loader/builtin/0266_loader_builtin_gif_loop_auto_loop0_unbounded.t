#!/bin/sh
# TAP test: loop=auto treats NETSCAPE loop0 as unbounded.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

. "${TOP_SRCDIR}/tests/_lib/sh/common.sh"

printf '1..1\n'
set -v

input_loop0="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-loop0.gif"

# Launch directly so $! points to the actual wrapper process.
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! -lauto -g "${input_loop0}" \
    -o /dev/null >/dev/null 2>&1 &
pid=$!

wait_limit=3
while test "${wait_limit}" -gt 0; do
    kill -0 "${pid}" 2>/dev/null || {
        wait "${pid}" 2>/dev/null || true
        echo "not ok" 1 - "loop auto treats NETSCAPE loop0 as unbounded unexpectedly finished"
        exit 0
    }
    sleep 1
    wait_limit=$((wait_limit - 1))
done

kill "${pid}" 2>/dev/null || true
wait "${pid}" 2>/dev/null || true

echo "ok" 1 - "loop auto treats NETSCAPE loop0 as unbounded"

exit 0
