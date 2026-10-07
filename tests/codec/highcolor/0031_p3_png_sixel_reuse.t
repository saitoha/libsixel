#!/bin/sh
# High-color P3 frame encoding accepts PNG intent and subsequent SIXEL reuse.
# Policy: docs/encoder/output/high-color/README.md
set -eux

echo "1..1"
set -v

SIXEL_TRACE_TOPIC=dither_contract ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" codec/highcolor/0031_p3_png_sixel_reuse || {
    echo "not ok 1 - high-color p3 png sixel reuse"
    exit 0
}
echo "ok 1 - high-color p3 png sixel reuse"
exit 0
