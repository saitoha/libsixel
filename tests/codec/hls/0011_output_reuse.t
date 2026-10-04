#!/bin/sh
# Native HLS output reuse.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0011_output_reuse || {
    echo "not ok 1 - Native HLS output reuse."
    exit 0
}
echo "ok 1 - Native HLS output reuse."
exit 0
