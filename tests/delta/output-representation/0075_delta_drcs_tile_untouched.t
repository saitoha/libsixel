#!/bin/sh
# A rejected DRCS selection leaves the existing character-output file intact.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0218_delta_drcs_tile_untouched \
    "${ARTIFACT_LOCAL_DIR}/tiles.txt" || {
    echo "not ok 1 - drcs tile untouched"
    exit 0
}
echo "ok 1 - drcs tile untouched"
exit 0
