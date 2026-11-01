#!/bin/sh
# Byte OR delta rejects before output; full-output reuse paints both black and white endpoints.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0211_delta_or_byte_recovery || {
    echo "not ok 1 - or byte recovery"
    exit 0
}
echo "ok 1 - or byte recovery"
exit 0
