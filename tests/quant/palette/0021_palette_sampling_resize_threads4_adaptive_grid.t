#!/bin/sh
# Verify a fourth thread restores sampling alongside resize work.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\\n"
    exit 0
}

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 -w 64 \
    -Qheckbert -d none -p 16 "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - four-thread resized palette encode failed"
    exit 0
}

test "${trace#*LSXSPL1\|requested=auto\|effective=adaptive-grid\|source=loaded-frame\|origin=auto\|phase=executed\|reason=resource-profile\|threads=4\|heavy=2\|budget_async=1\|job_ready=1*}" != "${trace}" || {
    echo "not ok 1 - resized four-thread sampling plan changed"
    exit 0
}
test "${trace#*LSXSMP1\|*}" != "${trace}" || {
    echo "not ok 1 - resized four-thread path skipped adaptive sampling"
    exit 0
}

echo "ok 1 - four threads admit sampling alongside resize work"
exit 0
