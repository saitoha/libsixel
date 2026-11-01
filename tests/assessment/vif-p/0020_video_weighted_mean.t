#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P unequal frame durations give mean 0.25.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -Ma -m VIF-P -J "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.jsonl" \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.png" \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.six") || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - unexpected VIF-P status"
    exit 0
}
test "$value" = 0.250000 || {
    echo "not ok 1 - VIF-P weighted mean"
    exit 0
}
echo "ok 1 - VIF-P unequal frame durations give mean 0.25"
exit 0
