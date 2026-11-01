#!/bin/sh
# P3 OR delta rejects before output; full-output reuse preserves native P3 application and opaque endpoints.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0213_delta_or_p3_recovery || {
    echo "not ok 1 - or p3 recovery"
    exit 0
}
echo "ok 1 - or p3 recovery"
exit 0
