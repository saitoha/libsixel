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

test "${trace_output#*"callback handoff decided mode=pipeline"}" \
    != "${trace_output}" || {
    echo "not ok 1 - coregraphics handoff pipeline by-ref trace assertions"
    exit 0
}

test "${trace_output#*"enqueue by-ref"}" != "${trace_output}" || {
    echo "not ok 1 - coregraphics handoff pipeline by-ref trace assertions"
    exit 0
}

test "${trace_output#*"worker clone fallback enabled"}" = "${trace_output}" || {
    echo "not ok 1 - coregraphics handoff pipeline by-ref trace assertions"
    exit 0
}

echo "ok 1 - coregraphics handoff pipeline by-ref trace"
exit 0
