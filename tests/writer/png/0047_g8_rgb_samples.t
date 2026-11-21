#!/bin/sh
# Policy: docs/writers/png/README.md
# G8 replicates gray in the RGB PNG.
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0047_g8_rgb_samples \
    "${ARTIFACT_LOCAL_DIR}/output.png" || {
    echo "not ok 1 - G8 replicates gray in the RGB PNG"
    exit 0
}
echo "ok 1 - G8 replicates gray in the RGB PNG"
exit 0
