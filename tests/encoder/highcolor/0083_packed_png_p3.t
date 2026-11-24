#!/bin/sh
# Packed alpha under high-color P3 PNG intent retains samples and omissions.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/highcolor/0083_packed_png_p3 || {
    printf '\n%s\n' "not ok 1 - packed alpha high-color P3 PNG intent"
    exit 0
}
printf '\n%s\n' "ok 1 - packed alpha high-color P3 PNG intent"
exit 0
