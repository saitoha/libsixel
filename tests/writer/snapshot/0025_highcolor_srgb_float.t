#!/bin/sh
# highcolor srgb float snapshot pixels and metadata.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_DECODER_INPUT_COLORSPACE=display-p3 \
    SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0025_highcolor_srgb_float "${ARTIFACT_LOCAL_DIR}" || {
    echo "not ok 1 - highcolor srgb float snapshot"
    exit 0
}
echo "ok 1 - highcolor srgb float snapshot"
exit 0
