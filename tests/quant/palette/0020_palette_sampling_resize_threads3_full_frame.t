#!/bin/sh
# Verify resize and its colorspace work consume two thread-budget units.

set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=3 -w 64 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - three-thread resized palette encode failed"
    exit 0
}

test "${trace#*LSXSPL1|requested=auto|effective=full-frame|source=preprocessed-frame|origin=auto|phase=executed|reason=resource-profile|threads=3|heavy=2|budget_async=0|job_ready=0*}" != "${trace}" || {
    echo "not ok 1 - resized three-thread sampling plan changed"
    exit 0
}
test "${trace#*LSXSMP1|*}" = "${trace}" || {
    echo "not ok 1 - resized three-thread path ran adaptive sampling"
    exit 0
}

echo "ok 1 - resize work keeps three threads on the full-frame path"
exit 0
