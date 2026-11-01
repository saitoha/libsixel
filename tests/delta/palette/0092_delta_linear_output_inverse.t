#!/bin/sh
# Independent linear output inverse preserves extended coordinates.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0119_delta_linear_output_inverse || {
    echo "not ok 1 - delta linear output inverse"
    exit 0
}
echo "ok 1 - delta linear output inverse"
exit 0
