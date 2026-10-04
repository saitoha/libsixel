#!/bin/sh
# Native HLS srgb rounding.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0003_srgb_rounding || {
    echo "not ok 1 - Native HLS srgb rounding."
    exit 0
}
echo "ok 1 - Native HLS srgb rounding."
exit 0
