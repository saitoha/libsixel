#!/bin/sh
# Verify a third thread restores sampling alongside colorspace work.

set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=3 \
    -Wlinear -Qheckbert -d none -p 16 "-~none" \
    -L builtin -ldisable "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - three-thread linear palette encode failed"
    exit 0
}

test "${trace#*LSXSPL1\|requested=auto\|effective=adaptive-grid\|source=loaded-frame\|origin=auto\|phase=executed\|reason=resource-profile\|threads=3\|heavy=1\|budget_async=1\|job_ready=1*}" != "${trace}" || {
    echo "not ok 1 - linear three-thread sampling plan changed"
    exit 0
}
test "${trace#*LSXSMP1\|*}" != "${trace}" || {
    echo "not ok 1 - linear three-thread path skipped adaptive sampling"
    exit 0
}

echo "ok 1 - three threads admit sampling beside colorspace work"
exit 0
