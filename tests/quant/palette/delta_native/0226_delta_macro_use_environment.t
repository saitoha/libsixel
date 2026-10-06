#!/bin/sh
# Environment-selected delta rejects automatic macro output.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_UPDATE_POLICY="delta" ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0226_delta_macro_use_environment || {
    echo "not ok 1 - macro use environment"
    exit 0
}
echo "ok 1 - macro use environment"
exit 0
