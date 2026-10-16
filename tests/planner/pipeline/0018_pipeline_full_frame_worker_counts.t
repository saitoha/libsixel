#!/bin/sh
# Policy: docs/threading/encoder.md
# Compare the verbose full-frame pipeline split with completed jobs and worker identity bounds.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP threading is disabled"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

# The original image provides six substantial dither jobs and 75 encode jobs.
# Keep geometry unchanged so the pre-scale plan describes the same raster.
plan=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_THREADS=8 \
    --env SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/pipeline.jsonl" \
    -4 full-frame -v -d fs "${TOP_SRCDIR}/images/snake.png" \
    2>&1 >/dev/null) || {
    echo "not ok 1 - full-frame conversion failed"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "planner/pipeline/0018_pipeline_full_frame_worker_counts" \
    "${ARTIFACT_LOCAL_DIR}/pipeline.jsonl" "${plan}" || {
    echo "not ok 1 - full-frame planned and observed worker counts differ"
    exit 0
}
echo "ok 1 - full-frame planned and observed worker counts agree"
exit 0
