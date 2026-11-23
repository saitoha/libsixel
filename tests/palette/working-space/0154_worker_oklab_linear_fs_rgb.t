#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Verify Oklab centroids and linear RGB raster FS on RGB input.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo 'ok 1 # SKIP thread backend unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader,palette_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=3 --precision-policy=float32 \
    -7 -Lbuiltin! -Ugamma -_none -~none -Fnone -trgb \
    -Qkmeans:seed=4 -p2 -Xoklab -Wlinear -aoff -dfs:scan=raster -0none \
    --binning-policy=none --sampling-policy=adaptive-grid \
    "${TOP_SRCDIR}/tests/data/color-numeric/mixed-spaces-rgb.ppm" 2>&1) || {
    printf '%s\n' "$trace" >&2
    echo 'not ok 1 - RGB mixed-space encode'
    exit 0
}
case "$trace" in *'LSXPJOB1|result=worker-palette'*) true ;; *) false ;; esac || {
    echo 'not ok 1 - adaptive sampling did not dispatch a palette job'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    mixed-spaces-rgb "$trace" || {
    echo 'not ok 1 - RGB Oklab centroids and linear diffusion pixels'
    exit 0
}
echo 'ok 1 - RGB Oklab centroids and linear diffusion pixels'
exit 0
