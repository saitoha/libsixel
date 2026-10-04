#!/bin/sh
# Native HLS srgb centroid.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0001_srgb_centroid || {
    echo "not ok 1 - Native HLS srgb centroid."
    exit 0
}
echo "ok 1 - Native HLS srgb centroid."
exit 0
