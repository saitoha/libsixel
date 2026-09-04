#!/bin/sh
# Verify bypassed sampling leaves both threads available to loader handoff.

set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=encode_handoff \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=2 \
    -b xterm16 -d none "-~none" -L builtin -ldisable -g \
    "${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png" \
    -o/dev/null 2>&1) || {
    echo "not ok 1 - built-in palette handoff encode failed"
    exit 0
}

test "${trace#*event=callback_handoff_decide handoff=pipeline*}" \
    != "${trace}" || {
    echo "not ok 1 - bypassed sampling reserved a palette worker"
    exit 0
}

echo "ok 1 - bypassed sampling leaves two threads for handoff"
exit 0
