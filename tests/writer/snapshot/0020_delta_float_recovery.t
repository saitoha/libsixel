#!/bin/sh
# PNG-selected float frames reject delta before palette work; full reuse preserves native application and exact endpoints.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/png_snapshot/0020_delta_float_recovery || {
    echo "not ok 1 - PNG delta float recovery"
    exit 0
}
echo "ok 1 - PNG delta float recovery"
exit 0
