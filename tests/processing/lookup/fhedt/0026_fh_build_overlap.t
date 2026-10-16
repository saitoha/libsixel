#!/bin/sh
# Verify build overlap with multiple active dither workers.
# Policy: docs/encoder/palette-application/lookup-policy/README.md
set -eux

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "1..0 # SKIP thread backend is unavailable"
    exit 0
}
echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_LOG_PATH="${ARTIFACT_LOCAL_DIR}/fhedt.jsonl"
export SIXEL_LOG_PATH
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    lookup/fhedt/0026_fh_build_overlap "${SIXEL_LOG_PATH}" || {
    echo "not ok 1 - build overlap"
    exit 0
}
echo "ok 1 - build overlap"
exit 0
