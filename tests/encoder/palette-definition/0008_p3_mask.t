#!/bin/sh
# Native HLS p3 mask.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0008_p3_mask || {
    printf '\n%s\n' "not ok 1 - Native HLS p3 mask."
    exit 0
}
printf '\n%s\n' "ok 1 - Native HLS p3 mask."
exit 0
