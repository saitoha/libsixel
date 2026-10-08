#!/bin/sh
# Verify refine byte.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" palette/delta_native/0234_delta_refine_byte || {
    echo "not ok 1 - refine_byte"
    exit 0
}
echo "ok 1 - refine_byte"
exit 0
