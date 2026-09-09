#!/bin/sh
# Verify a fifth thread restores sampling beside clip and resize work.

set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=5 \
    -c 64x64+0+0 -w 32 -Qheckbert -d none -p 16 "-~none" \
    -L builtin -ldisable "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - five-thread cropped resize encode failed"
    exit 0
}

test "${trace#*LSXSPL1\|requested=auto\|effective=adaptive-grid\|source=loaded-frame\|origin=auto\|phase=executed\|reason=resource-profile\|threads=5\|heavy=3\|budget_async=1\|job_ready=1*}" != "${trace}" || {
    echo "not ok 1 - cropped resize five-thread sampling plan changed"
    exit 0
}
test "${trace#*LSXSMP1\|*}" != "${trace}" || {
    echo "not ok 1 - cropped resize five-thread path skipped sampling"
    exit 0
}

echo "ok 1 - five threads admit sampling beside crop and resize"
exit 0
