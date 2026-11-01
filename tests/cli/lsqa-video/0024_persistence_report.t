#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -m S-CIELAB -L 'builtin!' --cms-engine=none -o "${ARTIFACT_LOCAL_DIR}/report.jsonl" -E "${ARTIFACT_LOCAL_DIR}/maps.f32" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null || {
    echo "not ok 1 - persistence report evaluation"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/clock_report_check 6 "${ARTIFACT_LOCAL_DIR}/report.jsonl" "${ARTIFACT_LOCAL_DIR}/maps.f32" || {
    echo "not ok 1 - persistence report assertion"
    exit 0
}
echo "ok 1 - persistence report"
exit 0
