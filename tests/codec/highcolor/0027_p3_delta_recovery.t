#!/bin/sh
# High-color P3 delta rejects before output and allows full-frame reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0027_p3_delta_recovery || {
    echo "not ok 1 - high-color p3 delta recovery"
    exit 0
}
echo "ok 1 - high-color p3 delta recovery"
exit 0
