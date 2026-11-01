#!/bin/sh
# highcolor p3 hls snapshot pixels and metadata.
# Policy: docs/writers/png/README.md
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_DECODER_INPUT_COLORSPACE=srgb \
    SIXEL_TRACE_TOPIC=dither_contract \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0027_highcolor_p3_hls "${ARTIFACT_LOCAL_DIR}" || {
    echo "not ok 1 - highcolor p3 hls snapshot"
    exit 0
}
echo "ok 1 - highcolor p3 hls snapshot"
exit 0
