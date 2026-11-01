#!/bin/sh
# Policy: docs/quality/metrics.md
# An ungated unavailable scalar video metric retains its runtime error.
set -eux
echo "1..1"
set -v

status=0
value=$(${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none \
    -m MS-SSIM "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" \
    "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six") || status=$?
test "${status}" -eq 4 && test -z "${value}" || {
    echo "not ok 1 - unavailable scalar video metric"
    exit 0
}
echo "ok 1 - unavailable scalar video metric"
exit 0
