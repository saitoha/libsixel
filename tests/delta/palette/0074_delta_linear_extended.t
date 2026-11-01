#!/bin/sh
# Verify delta linear extended.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0091_delta_linear_extended || {
    echo "not ok 1 - delta linear extended"
    exit 0
}
echo "ok 1 - delta linear extended"
exit 0
