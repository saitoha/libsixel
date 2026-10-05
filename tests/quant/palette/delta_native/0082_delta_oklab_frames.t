#!/bin/sh
# Verify delta oklab frames.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0082_delta_oklab_frames || {
    echo "not ok 1 - delta oklab frames"
    exit 0
}
echo "ok 1 - delta oklab frames"
exit 0
