#!/bin/sh
# Verify HLS byte or control.
# Policy: docs/encoder/output/output-colorspace.md
set -eux
echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0014_byte_or_control || {
    echo "not ok 1 - HLS byte or control"
    exit 0
}
echo "ok 1 - HLS byte or control"
exit 0
