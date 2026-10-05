#!/bin/sh
# Verify delta spatial dispatch.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0017_delta_spatial_dispatch || {
    echo "not ok 1 - delta spatial dispatch"
    exit 0
}
echo "ok 1 - delta spatial dispatch"
exit 0
