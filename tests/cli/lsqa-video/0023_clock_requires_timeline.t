#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v


status=0
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -Ac -m S-CIELAB -L 'builtin!' --cms-engine=none "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null || status=$?
test "$status" -eq 2 || {
    echo "not ok 1 - clock requires timeline"
    exit 0
}
echo "ok 1 - clock requires timeline"
exit 0
