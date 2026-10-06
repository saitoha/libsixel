#!/bin/sh
# Environment-selected delta also rejects OR output.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract SIXEL_UPDATE_POLICY="delta" ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0209_delta_or_environment || {
    echo "not ok 1 - or environment"
    exit 0
}
echo "ok 1 - or environment"
exit 0
