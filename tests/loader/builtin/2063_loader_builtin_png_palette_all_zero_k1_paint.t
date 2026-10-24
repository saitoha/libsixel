#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# PNG all zero-alpha SIXEL paint coverage, keycolor 1
set -eux
test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    set -v
    exit 0
}
echo "1..1"
set -v

output=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=1 \
    --gpu-policy=off --threading-policy=balanced:global_budget=1 --precision-policy=8bit \
    -Lbuiltin:cms_engine=none! -4full -5exact -_none -aoff -Fnone \
    -Qkmeans -Xgamma -Wgamma -Ugamma -dnone \
    "${TOP_SRCDIR}/tests/data/color-numeric/palette-trns-all.png") || {
    echo "not ok 1 - indexed PNG encode failed"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0191_png_palette_paint" 3 "$output" || {
    echo "not ok 1 - PNG all zero-alpha SIXEL paint coverage, keycolor 1"
    exit 0
}
echo "ok 1 - PNG all zero-alpha SIXEL paint coverage, keycolor 1"
exit 0
