#!/bin/sh
# Policy: docs/writers/png/README.md
# PAL8 index 255 retains the full PLTE.
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" writer/png/0046_pal8_index_255 \
    "${ARTIFACT_LOCAL_DIR}/output.png" || {
    echo "not ok 1 - PAL8 index 255 retains the full PLTE"
    exit 0
}
echo "ok 1 - PAL8 index 255 retains the full PLTE"
exit 0
