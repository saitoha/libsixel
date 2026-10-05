#!/bin/sh
# Verify palette export encoder isolation.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

actual=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    palette/export_capture/0006_encoder_isolation "${TOP_SRCDIR}/tests/data/inputs/snake_64.ppm") || {
    echo "not ok 1 - repeated palette export failed"
    exit 0
}
test "${actual}" = 'JASC-PAL
0100
4
0 0 0
85 85 85
170 170 170
255 255 255
JASC-PAL
0100
2
0 0 0
255 255 255
JASC-PAL
0100
4
0 0 0
85 85 85
170 170 170
255 255 255' || {
    echo "not ok 1 - exported palette count or values changed"
    exit 0
}
echo "ok 1 - palette export encoder isolation"
exit 0
