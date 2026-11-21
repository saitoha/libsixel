#!/bin/sh
# Policy: docs/writers/png/README.md
# PAL2 row padding stays out of the indexed PNG.
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0044_pal2_row_padding \
    "${ARTIFACT_LOCAL_DIR}/output.png" || {
    echo "not ok 1 - PAL2 row padding stays out of the indexed PNG"
    exit 0
}
echo "ok 1 - PAL2 row padding stays out of the indexed PNG"
exit 0
