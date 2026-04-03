#!/bin/sh
set -eux

test "${HAVE_COREGRAPHICS-}" = 1 || {
    printf "1..0 # SKIP coregraphics loader is unavailable\n"
    exit 0
}

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is unavailable\n"
    exit 0
}

echo "1..1"
set -v

trace_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env "SIXEL_THREADS=4" \
        --env "SIXEL_TRACE_TOPIC=encode_handoff" \
        -Lcoregraphics! -lauto -g \
        "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
        -o/dev/null 2>&1
) || {
    echo "not ok 1 - coregraphics handoff pipeline by-ref trace run"
    exit 0
}

printf '%s\n' "${trace_output}" | awk '
BEGIN {
    decided = 0
    byref = 0
    fallback = 0
}
index($0, "callback handoff decided mode=pipeline") { decided = 1 }
index($0, "enqueue by-ref") { byref = 1 }
index($0, "worker clone fallback enabled") { fallback = 1 }
END {
    if (decided != 1 || byref != 1 || fallback != 0) {
        exit 1
    }
}
' || {
    echo "not ok 1 - coregraphics handoff pipeline by-ref trace assertions"
    exit 0
}

echo "ok 1 - coregraphics handoff pipeline by-ref trace"
exit 0
