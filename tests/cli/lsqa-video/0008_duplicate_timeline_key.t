#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

set +e
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -m S-CIELAB -J "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-invalid.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-held.six" >/dev/null
status=$?
set -e
test "${status}" -eq 4 || {
    printf '\n%s\n' "not ok 1 - duplicate_timeline_key"
    exit 0
}

printf '\n%s\n' "ok 1 - duplicate timeline key"
exit 0
