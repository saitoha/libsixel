#!/bin/sh
# Verify delta a band.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0030_delta_a_band || {
    echo "not ok 1 - delta a band"
    exit 0
}
echo "ok 1 - delta a band"
exit 0
