#!/bin/sh
# Verify accept raster soft.
# Policy: docs/encoder/palette-application/dithering/sierra1-gpu.md
set -eux
echo "1..1"
set -v

status=0
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "dither/sierra1-gpu/0001_accept_raster_soft" || status=$?
test "$status" -eq 0 || {
    echo "not ok 1 - accept raster soft"
    exit 0
}
echo "ok 1 - accept raster soft"
exit 0
