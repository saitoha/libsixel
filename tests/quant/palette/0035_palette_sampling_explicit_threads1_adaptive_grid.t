#!/bin/sh
# Verify explicit adaptive sampling uses the loaded frame without a worker.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -4adaptive-grid \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - explicit single-thread adaptive sampling failed"
    exit 0
}

test "${trace#*LSXSPL1\|requested=adaptive-grid\|effective=adaptive-grid\|source=loaded-frame\|origin=explicit\|phase=executed\|reason=explicit\|threads=1\|heavy=0\|budget_async=0\|job_ready=0*}" != "${trace}" || {
    echo "not ok 1 - explicit sampling plan changed in single-thread mode"
    exit 0
}

test "${trace#*LSXSMP1\|*}" != "${trace}" || {
    echo "not ok 1 - explicit adaptive sampling did not execute"
    exit 0
}

echo "ok 1 - explicit adaptive sampling uses loaded-frame input synchronously"
exit 0
