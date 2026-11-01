#!/bin/sh
# Policy: docs/quality/metrics.md
# VIF-P temporal extrema remain 0 and 1.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none \
    -Ma  -J "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.jsonl" \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.png" \
    "${TOP_SRCDIR}/tests/data/assessment/vif-p-video.six") || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - unexpected VIF-P status"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/vif_video_summary "$value" || {
    echo "not ok 1 - VIF-P temporal extrema"
    exit 0
}
echo "ok 1 - VIF-P temporal extrema remain 0 and 1"
exit 0
