#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
LSQA_PERSISTENCE_TIME=0.2 ${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -t0 -m S-CIELAB -L 'builtin!' --cms-engine=none -o "${ARTIFACT_LOCAL_DIR}/report.jsonl" -E "${ARTIFACT_LOCAL_DIR}/maps.f32" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null || {
    echo "not ok 1 - persistence cli zero evaluation"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/clock_report_check 7 "${ARTIFACT_LOCAL_DIR}/report.jsonl" "${ARTIFACT_LOCAL_DIR}/maps.f32" || {
    echo "not ok 1 - persistence cli zero assertion"
    exit 0
}
echo "ok 1 - persistence cli zero"
exit 0
