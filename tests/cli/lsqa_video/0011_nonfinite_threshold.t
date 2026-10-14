#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -unan "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null
status=$?
set -e
test "${status}" -eq 2 || {
    echo "not ok 1 - nonfinite rejection"
    exit 0
}

echo "ok 1 - nonfinite threshold"
exit 0
