#!/bin/sh
# Policy: docs/encoder/palette-application/working-colorspace/README.md
# Verify Oklab centroids and linear RGB raster FS on XYZ input.
set -eux
echo '1..1'
set -v

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP img2sixel unavailable'
    exit 0
}
test "${HAVE_LIBTIFF-}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP libtiff unavailable'
    exit 0
}
test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf '\n%s\n' 'ok 1 # SKIP thread backend unavailable'
    exit 0
}
trace=$(SIXEL_TRACE_TOPIC=loader,palette_contract ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threading-policy=greedy:global_budget=3 --precision-policy=float32 \
    -7 -Llibtiff! -Usrgb -_none -~none -Fnone -trgb \
    -Qkmeans:seed=4 -p2 -Xoklab -Wsrgb-linear -aoff -dfs:scan=raster -0none \
    --binning-policy=none --sampling-policy=adaptive-grid \
    "${TOP_SRCDIR}/tests/data/color-numeric/mixed-spaces-xyz.tif" 2>&1) || {
    printf '%s\n' "$trace" >&2
    printf '\n%s\n' 'not ok 1 - XYZ mixed-space encode'
    exit 0
}
case "$trace" in *'LSXPJOB1|result=worker-palette'*) true ;; *) false ;; esac || {
    printf '\n%s\n' 'not ok 1 - adaptive sampling did not dispatch a palette job'
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cli/0036_cli_color_wire \
    mixed-spaces-xyz "$trace" || {
    printf '\n%s\n' 'not ok 1 - XYZ Oklab centroids and linear diffusion pixels'
    exit 0
}
printf '\n%s\n' 'ok 1 - XYZ Oklab centroids and linear diffusion pixels'
exit 0
