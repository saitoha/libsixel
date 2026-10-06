#!/bin/sh
# Float OR delta rejects before output; full-output reuse preserves native float application and opaque endpoints.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0212_delta_or_float_recovery || {
    echo "not ok 1 - or float recovery"
    exit 0
}
echo "ok 1 - or float recovery"
exit 0
