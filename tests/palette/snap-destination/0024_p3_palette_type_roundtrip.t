#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify frame API AUTO/RGB, HLS and AUTO/RGB output round trips.
set -eux

echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/0054_p3_encoder_contract palette-auto || {
    echo "not ok 1 - P3 palette AUTO and HLS round trip"
    exit 0
}
echo "ok 1 - P3 palette AUTO and HLS round trip"
exit 0
