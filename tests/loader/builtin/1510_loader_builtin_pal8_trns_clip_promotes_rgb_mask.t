#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify clip geometry promotes PAL8+transparent input to RGB work storage.
# Policy: docs/concepts/pixelformat.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
clip_log=""
clip_rgb=0

clip_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    -A composite -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -c2x1+0+0 "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - clipped pal8 transparent render failed"
    exit 0
}

case "${clip_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        clip_rgb=1
        ;;
esac

test "${clip_rgb}" = 1 || {
    echo "not ok 1 - clip path did not promote to rgb888 work format"
    exit 0
}

echo "ok 1 - clip geometry promotes pal8 transparent input to rgb storage"
exit 0
