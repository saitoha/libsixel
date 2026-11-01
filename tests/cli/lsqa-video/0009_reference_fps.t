#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -L 'builtin!' --cms-engine=none -f30000/1001 -m S-CIELAB -o "${ARTIFACT_LOCAL_DIR}/frames.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null || {
    echo "not ok 1 - fps override"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/video_report_check 9 "${ARTIFACT_LOCAL_DIR}/frames.jsonl" || {
    echo "not ok 1 - frame report assertion"
    exit 0
}

echo "ok 1 - reference fps"
exit 0
