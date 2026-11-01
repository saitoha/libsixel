#!/bin/sh
# A seeded linear surface follows transfer/definition identity across fixed-palette frames.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0129_delta_linear_output_identity || {
    echo "not ok 1 - delta linear output identity"
    exit 0
}
echo "ok 1 - delta linear output identity"
exit 0
