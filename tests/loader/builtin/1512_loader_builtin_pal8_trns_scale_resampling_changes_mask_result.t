#!/bin/sh
# Verify PAL8 transparent scale output changes with resampling mode.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v

input_png="${TOP_SRCDIR}/tests/data/inputs/formats/pal8-trns-key0.png"
nearest_log=""
bilinear_log=""
nearest_out=""
bilinear_out=""
nearest_rgb=0
bilinear_rgb=0

nearest_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r nearest "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - nearest scaled render failed"
    exit 0
}

bilinear_log=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r bilinear "${input_png}" 2>&1 >/dev/null) || {
    echo "not ok 1 - bilinear scaled render failed"
    exit 0
}

nearest_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r nearest "${input_png}") || {
    echo "not ok 1 - nearest scaled sixel output failed"
    exit 0
}

bilinear_out=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -d fs:scan=raster \
    -w7 -h3 -r bilinear "${input_png}") || {
    echo "not ok 1 - bilinear scaled sixel output failed"
    exit 0
}

case "${nearest_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        nearest_rgb=1
        ;;
esac

case "${bilinear_log}" in
    *"formats: source=rgb888 work=rgb888"*)
        bilinear_rgb=1
        ;;
esac

test "${nearest_rgb}" = 1 || {
    echo "not ok 1 - nearest path did not promote to rgb888 work format"
    exit 0
}

test "${bilinear_rgb}" = 1 || {
    echo "not ok 1 - bilinear path did not promote to rgb888 work format"
    exit 0
}

test "${nearest_out}" != "${bilinear_out}" || {
    echo "not ok 1 - nearest and bilinear outputs are unexpectedly identical"
    exit 0
}

echo "ok 1 - nearest and bilinear scaled outputs diverge for pal8 transparency"
exit 0
