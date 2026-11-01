#!/bin/sh
# PNG-selected P3 frames reject delta before palette work; full reuse preserves native P3 application and exact endpoints.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0021_delta_p3_recovery || {
    echo "not ok 1 - PNG delta p3 recovery"
    exit 0
}
echo "ok 1 - PNG delta p3 recovery"
exit 0
