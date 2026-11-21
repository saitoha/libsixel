#!/bin/sh
# Policy: docs/writers/png/README.md
# G8 uses the supplied palette for the RGB PNG.
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0048_g8_palette_lookup \
    "${ARTIFACT_LOCAL_DIR}/output.png" || {
    echo "not ok 1 - G8 uses the supplied palette for the RGB PNG"
    exit 0
}
echo "ok 1 - G8 uses the supplied palette for the RGB PNG"
exit 0
