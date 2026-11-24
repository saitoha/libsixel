#!/bin/sh
# highcolor srgb byte snapshot pixels and metadata.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_DECODER_INPUT_COLORSPACE=display-p3 \
    SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0024_highcolor_srgb_byte "${ARTIFACT_LOCAL_DIR}" || {
    printf '\n%s\n' "not ok 1 - highcolor srgb byte snapshot"
    exit 0
}
printf '\n%s\n' "ok 1 - highcolor srgb byte snapshot"
exit 0
