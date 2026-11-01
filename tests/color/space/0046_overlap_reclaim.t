#!/bin/sh
# Policy: docs/threading/encoder.md
# Observe returned palette slots in the working-space conversion.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/reclaim.jsonl"
SIXEL_GPU_POLICY=off
export SIXEL_LOG_PATH SIXEL_GPU_POLICY
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/parallel/0008_overlap_reclaim" "${SIXEL_LOG_PATH}" || {
    echo "not ok 1 - palette slots join working conversion after completion"
    exit 0
}
echo "ok 1 - palette slots join working conversion after completion"
exit 0
