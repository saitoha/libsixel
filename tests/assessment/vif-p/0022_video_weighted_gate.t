#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P bounds reject weighted 0.25 despite plain mean 0.5.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -Ma -m VIF-P -b VIF-P:min=0.3 -J "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.jsonl" \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.png" \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.six") || status=$?
test "$status" -eq 5 || {
    echo "not ok 1 - unexpected VIF-P status"
    exit 0
}
test "$value" = 0.250000 || {
    echo "not ok 1 - failed gate reported an incorrect value"
    exit 0
}
echo "ok 1 - VIF-P bounds reject weighted 0.25 despite plain mean 0.5"
exit 0
