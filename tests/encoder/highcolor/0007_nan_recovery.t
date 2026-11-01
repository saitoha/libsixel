#!/bin/sh
# NaN input rejects before output and the same float context remains reusable.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0007_nan_recovery || {
    echo "not ok 1 - high-color nan recovery"
    exit 0
}
echo "ok 1 - high-color nan recovery"
exit 0
