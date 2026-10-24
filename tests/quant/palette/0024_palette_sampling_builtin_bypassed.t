#!/bin/sh
# Verify fixed built-in palettes bypass the sampling stage.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\\n"
    exit 0
}

echo "1..1"
set -v

# Keep this scheduling contract free of loader color-conversion work.
trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=balanced:global_budget=1 \
    -b xterm16 -d none "-~none" --cms-engine=none -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - built-in palette encode failed"
    exit 0
}

case "${trace}" in *LSXSPL1\|requested=auto\|effective=unset\|source=none\|origin=auto\|phase=bypassed\|reason=not-applicable\|threads=1\|heavy=0\|budget_async=0\|job_ready=0**) true ;; *) false ;; esac || {
    echo "not ok 1 - built-in palette sampling was not bypassed"
    exit 0
}
case "${trace}" in *LSXSMP1\|**) false ;; *) true ;; esac || {
    echo "not ok 1 - built-in palette unexpectedly ran sampling"
    exit 0
}

echo "ok 1 - built-in palette bypasses sampling"
exit 0
