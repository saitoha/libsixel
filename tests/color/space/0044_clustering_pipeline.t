#!/bin/sh
# Policy: docs/threading/encoder.md
# Verify clustering pipeline for the common float transaction.
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/colorspace.jsonl"
export SIXEL_LOG_PATH
SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS=1
export SIXEL_COLORSPACE_PARALLEL_MIN_PIXELS
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "colorspace/parallel/0006_clustering_pipeline" "${SIXEL_LOG_PATH}" || {
    echo "not ok 1 - clustering pipeline"
    exit 0
}
echo "ok 1 - clustering pipeline"
exit 0
