#!/bin/sh
# High-color P3 OR mode rejects before output and allows ordinary reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0028_p3_or_recovery || {
    echo "not ok 1 - high-color p3 or recovery"
    exit 0
}
echo "ok 1 - high-color p3 or recovery"
exit 0
