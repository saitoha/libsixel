#!/bin/sh
# Native HLS hue and gamut.
# Policy: docs/encoder/output/output-colorspace.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    codec/hls/0005_hue_and_gamut || {
    echo "not ok 1 - Native HLS hue and gamut."
    exit 0
}
echo "ok 1 - Native HLS hue and gamut."
exit 0
