#!/bin/sh
# Policy: docs/quality/metrics.md
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -Ma -J "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-stale.jsonl" -b S-CIEDE2000_max:max=0 "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-stale.six" >/dev/null || status=$?
test "${status}" -eq 5 || {
    echo "not ok 1 - spatial DE00 video ceiling"
    exit 0
}

echo "ok 1 - spatial DE00 video ceiling rejects stale frames"
exit 0
