#!/bin/sh
# Policy: docs/writers/png/README.md
# PAL1 row padding stays out of the indexed PNG.
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0043_pal1_row_padding \
    "${ARTIFACT_LOCAL_DIR}/output.png" || {
    printf '\n%s\n' "not ok 1 - PAL1 row padding stays out of the indexed PNG"
    exit 0
}
printf '\n%s\n' "ok 1 - PAL1 row padding stays out of the indexed PNG"
exit 0
