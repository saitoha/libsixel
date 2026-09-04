#!/bin/sh
# Verify colorspace work consumes the spare palette-worker thread.

set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=2 \
    -Wlinear -Qheckbert -d none -p 16 "-~none" \
    -L builtin -ldisable "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - two-thread linear palette encode failed"
    exit 0
}

test "${trace#*LSXSPL1|requested=auto|effective=full-frame|source=preprocessed-frame|origin=auto|phase=executed|reason=resource-profile|threads=2|heavy=1|budget_async=0|job_ready=0*}" != "${trace}" || {
    echo "not ok 1 - linear two-thread sampling plan changed"
    exit 0
}
test "${trace#*LSXSMP1|*}" = "${trace}" || {
    echo "not ok 1 - linear two-thread path ran adaptive sampling"
    exit 0
}

echo "ok 1 - colorspace work keeps two threads on the full-frame path"
exit 0
