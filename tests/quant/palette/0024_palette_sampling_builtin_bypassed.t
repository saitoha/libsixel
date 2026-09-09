#!/bin/sh
# Verify fixed built-in palettes bypass the sampling stage.

set -eux

echo "1..1"
set -v

trace=$(set +xv; SIXEL_TRACE_TOPIC=palette_contract \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 \
    -b xterm16 -d none "-~none" -L builtin -ldisable \
    "${TOP_SRCDIR}/images/snake.png" 2>&1 >/dev/null) || {
    echo "not ok 1 - built-in palette encode failed"
    exit 0
}

test "${trace#*LSXSPL1|requested=auto|effective=unset|source=none|origin=auto|phase=bypassed|reason=not-applicable|threads=1|heavy=0|budget_async=0|job_ready=0*}" != "${trace}" || {
    echo "not ok 1 - built-in palette sampling was not bypassed"
    exit 0
}
test "${trace#*LSXSMP1|*}" = "${trace}" || {
    echo "not ok 1 - built-in palette unexpectedly ran sampling"
    exit 0
}

echo "ok 1 - built-in palette bypasses sampling"
exit 0
