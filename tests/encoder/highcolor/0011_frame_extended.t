#!/bin/sh
# The high-level frame entry preserves extended linear samples for SMPTE-C output.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0011_frame_extended || {
    printf '\n%s\n' "not ok 1 - high-color frame extended"
    exit 0
}
printf '\n%s\n' "ok 1 - high-color frame extended"
exit 0
