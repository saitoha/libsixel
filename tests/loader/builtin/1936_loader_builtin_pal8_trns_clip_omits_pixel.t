#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify that clipping preserves omitted transparent coverage after promotion.
# Policy: docs/concepts/pixelformat.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
clip_out=""
omitted_then_painted=0

clip_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -c2x1+0+0 "${input_png}") || {
    echo "not ok 1 - clipped pal8 transparent output failed"
    exit 0
}

case "${clip_out}" in
    *"#1?@"*)
        omitted_then_painted=1
        ;;
esac

test "${omitted_then_painted}" = 1 || {
    echo "not ok 1 - clipped output did not omit transparent first pixel"
    exit 0
}

echo "ok 1 - clipped promoted input retains omitted transparent coverage"
exit 0
