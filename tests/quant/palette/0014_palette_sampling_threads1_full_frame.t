#!/bin/sh
# Verify the single-thread legacy path uses the preprocessed full frame.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

# Keep this scheduling contract free of loader color-conversion work.
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -Qheckbert -d none -p 16 "-~none" --cms-engine=none -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - single-thread palette encode failed"
    exit 0
}

case "${trace}" in *LSXSPL1\|requested=auto\|effective=full-frame\|source=preprocessed-frame\|origin=auto\|phase=executed\|reason=resource-profile\|threads=1\|heavy=0\|budget_async=0\|job_ready=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - single-thread sampling plan changed"
    exit 0
}
case "${trace}" in *LSXSMP1\|**) false ;; *) true ;; esac || {
    echo "not ok 1 - single-thread path unexpectedly ran adaptive sampling"
    exit 0
}

echo "ok 1 - single-thread path uses the preprocessed full frame"
exit 0
