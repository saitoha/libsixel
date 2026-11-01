#!/bin/sh
# Verify delta linear compare.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0087_delta_linear_compare || {
    echo "not ok 1 - delta linear compare"
    exit 0
}
echo "ok 1 - delta linear compare"
exit 0
