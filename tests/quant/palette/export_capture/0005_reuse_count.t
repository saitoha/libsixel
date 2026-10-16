#!/bin/sh
# Verify palette export reuse count.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

echo "1..1"
set -v

# The equal-weight centroid oracle requires the full pixel population;
# adaptive solid-region weights are a separate sampling contract.
actual=$(${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env SIXEL_SAMPLING_POLICY=full-frame \
    palette/export_capture/0005_reuse_count "${TOP_SRCDIR}/tests/data/inputs/small.ppm") || {
    echo "not ok 1 - repeated palette export failed"
    exit 0
}
# The first six primary/secondary entries may occur in solver order.
# Heckbert splits the six equal-weight colors on R; the two centers
# have G=B=0.5 exactly, so the sRGB8 means are 128.
case "${actual}" in
    'JASC-PAL
0100
6
'*'
JASC-PAL
0100
2
0 128 128
255 128 128') ;;
    *)
        echo "not ok 1 - reused export has a stale count or wrong centroid"
        exit 0
        ;;
esac
echo "ok 1 - palette export reuse count"
exit 0
