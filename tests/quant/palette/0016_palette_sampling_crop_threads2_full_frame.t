#!/bin/sh
# Verify crop work consumes the spare thread needed by palette sampling.

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

# Keep this scheduling contract free of loader color-conversion work.
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=2 \
    -c 64x64+0+0 -Qheckbert -d none -p 16 "-~none" \
    --cms-engine=none -L builtin -ldisable "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - two-thread cropped palette encode failed"
    exit 0
}

case "${trace}" in *LSXSPL1\|requested=auto\|effective=full-frame\|source=preprocessed-frame\|origin=auto\|phase=executed\|reason=resource-profile\|threads=2\|heavy=1\|budget_async=0\|job_ready=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - cropped two-thread sampling plan changed"
    exit 0
}
case "${trace}" in *LSXSMP1\|**) false ;; *) true ;; esac || {
    echo "not ok 1 - cropped two-thread path ran adaptive sampling"
    exit 0
}

echo "ok 1 - crop work keeps two threads on the full-frame path"
exit 0
