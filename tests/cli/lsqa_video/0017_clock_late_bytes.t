#!/bin/sh
# Policy: docs/quality/video-evaluation.md
set -eux
echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${LSQA_PATH}" -Ma -Ac -C "${TOP_SRCDIR}/tests/data/assessment/video-snake-16-clock.jsonl" -m S-CIELAB -L 'builtin!' --cms-engine=none -o "${ARTIFACT_LOCAL_DIR}/report.jsonl" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.png" "${TOP_SRCDIR}/tests/data/assessment/video-snake-16.six" >/dev/null || {
    echo "not ok 1 - clock late bytes evaluation"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" assessment/clock_report_check 3 "${ARTIFACT_LOCAL_DIR}/report.jsonl" || {
    echo "not ok 1 - clock late bytes assertion"
    exit 0
}
echo "ok 1 - clock late bytes"
exit 0
