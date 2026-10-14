#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -f30 -J "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-held.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-held.six" >/dev/null
status=$?
set -e
test "${status}" -eq 2 || {
    echo "not ok 1 - conflicting time sources"
    exit 0
}

echo "ok 1 - timeline fps conflict"
exit 0
