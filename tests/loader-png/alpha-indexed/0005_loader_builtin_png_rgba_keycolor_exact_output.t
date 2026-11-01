#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png/README.md
# Verify both RGBA keycolor settings preserve exact source-color placement.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}
echo "1..1"
set -v

# The two RGBA8 pixels are (10,20,30,40) and (50,60,70,255).
# No background is present: nonzero alpha does not premultiply source RGB.
# Exact gamma samples with no palette adjustments map each byte x to
# floor((100*x+127)/255). The two '@' data bytes paint positions 0 and 1.
# Either palette-index ordering represents the same two painted pixels.
esc=$(printf '\033')
expected="${esc}Pq\"1;1;2;1#0;2;20;24;27#1;2;4;8;12#1@#0@${esc}\\"
reordered="${esc}Pq\"1;1;2;1#0;2;4;8;12#1;2;20;24;27#0@#1@${esc}\\"
first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=0 \
    --gpu-policy=off --threading-policy=greedy:global_budget=1 --precision-policy=8bit \
    -Lbuiltin:cms_engine=none! -4full -5exact -_none -aoff -Fnone \
    -Qkmeans -Xgamma -Wgamma -Ugamma -dnone \
    "${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png") || {
    echo "not ok 1 - ordinary RGBA render failed"
    exit 0
}
test "$first" = "$expected" || test "$first" = "$reordered" || {
    echo "not ok 1 - ordinary RGBA path changed source-color placement"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_LIBPNG_USE_TRNS_KEYCOLOR=2 \
    --gpu-policy=off --threading-policy=greedy:global_budget=1 --precision-policy=8bit \
    -Lbuiltin:cms_engine=none! -4full -5exact -_none -aoff -Fnone \
    -Qkmeans -Xgamma -Wgamma -Ugamma -dnone \
    "${TOP_SRCDIR}/tests/data/inputs/formats/rgba.png") || {
    echo "not ok 1 - keycolor RGBA render failed"
    exit 0
}
test "$second" = "$expected" || test "$second" = "$reordered" || {
    echo "not ok 1 - keycolor RGBA path changed source-color placement"
    exit 0
}
echo "ok 1 - both RGBA keycolor paths preserve source-color placement"
exit 0
