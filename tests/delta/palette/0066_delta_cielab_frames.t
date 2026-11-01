#!/bin/sh
# Verify delta cielab frames.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0083_delta_cielab_frames || {
    echo "not ok 1 - delta cielab frames"
    exit 0
}
echo "ok 1 - delta cielab frames"
exit 0
