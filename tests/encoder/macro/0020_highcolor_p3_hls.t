#!/bin/sh
# Verify highcolor p3 hls through the actual hexadecimal callback.
# Policy: docs/encoder/output/terminal-macros.md
set -eux

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"
SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/macro/0020_highcolor_p3_hls "${ARTIFACT_LOCAL_DIR}" || {
    echo "not ok 1 - highcolor p3 hls"
    exit 0
}
echo "ok 1 - highcolor p3 hls"
exit 0
