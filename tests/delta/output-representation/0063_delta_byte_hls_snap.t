#!/bin/sh
# Explicit exact snap is reflected in byte HLS candidate comparison.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0199_delta_byte_hls_snap || {
    printf '\n%s\n' "not ok 1 - delta byte hls snap"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte hls snap"
exit 0
