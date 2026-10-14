#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -m S-CIELAB  "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-held.six" >/dev/null
status=$?
set -e
test "${status}" -eq 4 || {
    echo "not ok 1 - frame_count_mismatch"
    exit 0
}

echo "ok 1 - frame count mismatch"
exit 0
