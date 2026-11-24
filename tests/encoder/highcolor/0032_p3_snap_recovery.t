#!/bin/sh
# High-color P3 active snap rejects before output and allows unsnapped reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0032_p3_snap_recovery || {
    printf '\n%s\n' "not ok 1 - high-color p3 snap recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color p3 snap recovery"
exit 0
