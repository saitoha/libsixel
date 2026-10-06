#!/bin/sh
# High-color P3 DRCS rejects before output and allows ordinary reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0029_p3_drcs_recovery || {
    echo "not ok 1 - high-color p3 drcs recovery"
    exit 0
}
echo "ok 1 - high-color p3 drcs recovery"
exit 0
