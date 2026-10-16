#!/bin/sh
# Verify clip, resize, and colorspace work consume three budget units.

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
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=4 \
    -c 64x64+0+0 -w 32 -Qheckbert -d none -p 16 "-~none" \
    -L builtin -ldisable "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - four-thread cropped resize encode failed"
    exit 0
}

case "${trace}" in *LSXSPL1\|requested=auto\|effective=adaptive-grid\|source=loaded-frame\|origin=auto\|phase=executed\|reason=sampling-default\|threads=4\|heavy=3\|budget_async=0\|job_ready=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - cropped resize four-thread sampling plan changed"
    exit 0
}
case "${trace}" in *LSXSMP1\|**) true ;; *) false ;; esac || {
    echo "not ok 1 - synchronous adaptive sampling was not executed"
    exit 0
}

echo "ok 1 - crop and resize use synchronous adaptive sampling"
exit 0
