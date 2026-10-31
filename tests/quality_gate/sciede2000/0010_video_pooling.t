#!/bin/sh
# Policy: docs/quality/metrics.md
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${LSQA_PATH}" -L 'builtin!' --cms-engine=none -Ma -J "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-stale.jsonl" --ppd=60 -m S-CIEDE2000,S-CIEDE2000_max -o "${ARTIFACT_LOCAL_DIR}/frames.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-stale.six" >/dev/null || {
    echo "not ok 1 - spatial DE00 video report"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/video_report_check 15 "${ARTIFACT_LOCAL_DIR}/frames.jsonl" || {
    echo "not ok 1 - spatial DE00 video pooling"
    exit 0
}

echo "ok 1 - spatial DE00 video duration weighting, temporal maxima and metadata"
exit 0
