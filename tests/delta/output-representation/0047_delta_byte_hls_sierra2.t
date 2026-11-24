#!/bin/sh
# Byte HLS sierra2 captures and compares independent emitted colors without float promotion.
# Policy: docs/encoder/output/delta-encoding.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/delta_native/0182_delta_byte_hls_sierra2 || {
    printf '\n%s\n' "not ok 1 - delta byte hls sierra2"
    exit 0
}
printf '\n%s\n' "ok 1 - delta byte hls sierra2"
exit 0
