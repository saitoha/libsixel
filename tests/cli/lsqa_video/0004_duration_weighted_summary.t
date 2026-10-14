#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -m S-CIELAB -o "${ARTIFACT_LOCAL_DIR}/frames.jsonl" -J "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-stale.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-stale.six" >/dev/null || {
    echo "not ok 1 - stale sequence"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/video_report_check 4 "${ARTIFACT_LOCAL_DIR}/frames.jsonl" || {
    echo "not ok 1 - frame report assertion"
    exit 0
}

echo "ok 1 - duration weighted summary"
exit 0
