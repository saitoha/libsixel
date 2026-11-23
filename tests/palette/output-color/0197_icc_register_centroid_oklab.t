#!/bin/sh
# Policy: docs/encoder/output/output-colorspace.md
# Verify the emitted ICC palette against independent working-space values.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
data="${TOP_SRCDIR}/tests/data/color-numeric"
export SIXEL_LOADER_CMS_BPC=0 SIXEL_LOADER_CMS_RENDERING_INTENT=relative
trace=$(SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=1 --precision-policy=float32 \
    -7 -Lbuiltin:Ebuiltin! -U "icc:P${data}/output-bpc.icc:bpc=0" \
    -_none -~none -Fnone -trgb -aoff -0none -dnone \
    -Xoklab -Wlinear -Qkmeans:seed=1 -p2 --binning-policy=none \
    --sampling-policy=full-frame "${data}/icc-register-centroid.ppm" 2>&1) || {
    printf '%s\n' "$trace" >&2
    echo 'not ok 1 - encode projected ICC palette'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    icc-register-centroid-oklab "$trace" || {
    echo 'not ok 1 - projected ICC palette colors and indices'
    exit 0
}
echo 'ok 1 - projected ICC palette colors and indices'
exit 0
