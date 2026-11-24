#!/bin/sh
# Infinity input rejects before output and preserves the input format for reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0008_infinity_recovery || {
    printf '\n%s\n' "not ok 1 - high-color infinity recovery"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color infinity recovery"
exit 0
