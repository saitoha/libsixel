#!/bin/sh
# An undersized source mask rejects before output and allows opaque reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0026_p3_short_mask_recovery || {
    echo "not ok 1 - high-color p3 short mask recovery"
    exit 0
}
echo "ok 1 - high-color p3 short mask recovery"
exit 0
