#!/bin/sh
# High-color P3 macro output rejects before output and allows ordinary reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0030_p3_macro_recovery || {
    echo "not ok 1 - high-color p3 macro recovery"
    exit 0
}
echo "ok 1 - high-color p3 macro recovery"
exit 0
