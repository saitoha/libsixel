#!/bin/sh
# TAP test: unknown NETSCAPE loop subtype does not cause force-loop hang.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-netscape-unknown-subtype.gif"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 -Lbuiltin! -lforce -d none:scan=raster -p 256 \
    "${input_gif}" >/dev/null 2>/dev/null &
pid=$!

# Fast path: short polling for common completion.
wait_limit=10
while test "${wait_limit}" -gt 0; do
    kill -0 "${pid}" 2>/dev/null || {
        break
    }
    sleep 0.02
    wait_limit=$((wait_limit - 1))
done

kill -0 "${pid}" 2>/dev/null && {
    # Fallback path: preserve the old watchdog as insurance.
    wait_limit=5
    while test "${wait_limit}" -gt 0; do
        kill -0 "${pid}" 2>/dev/null || {
            break
        }
        sleep 1
        wait_limit=$((wait_limit - 1))
    done
}

kill -0 "${pid}" 2>/dev/null && {
    kill "${pid}" 2>/dev/null || true
    wait "${pid}" 2>/dev/null || true
    echo "not ok" 1 - "builtin unknown NETSCAPE subtype caused force-loop hang"
    exit 0
}

wait "${pid}" || rc=$?
test "${rc-0}" -eq 0 || {
    echo "not ok" 1 - "builtin unknown NETSCAPE subtype decode failed"
    exit 0
}

echo "ok" 1 - "builtin unknown NETSCAPE subtype does not hang in force-loop"
exit 0
