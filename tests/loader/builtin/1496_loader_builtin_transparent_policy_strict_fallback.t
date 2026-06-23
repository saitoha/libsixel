#!/bin/sh
# Verify transparent policy strict parsing and invalid fallback behavior.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_bmp="${TOP_SRCDIR}/tests/data/inputs/formats/bmp-info40-bi-png-rgba16-2x2.bmp"

out_default=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - default transparent policy render failed"
    exit 0
}

out_composite=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRANSPARENT_POLICY=composite \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - composite transparent policy render failed"
    exit 0
}

out_invalid=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRANSPARENT_POLICY=invalid \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - invalid transparent policy render failed"
    exit 0
}

out_transparent=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRANSPARENT_POLICY=transparent \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - transparent policy render failed"
    exit 0
}

test "${out_default}" != "${out_composite}" || {
    echo "not ok 1 - explicit composite policy did not change output"
    exit 0
}

test "${out_default}" = "${out_invalid}" || {
    echo "not ok 1 - invalid transparent policy did not fall back to default"
    exit 0
}

test "${out_default}" = "${out_transparent}" || {
    echo "not ok 1 - default policy mismatch against explicit transparent"
    exit 0
}

echo "ok 1 - transparent policy strict parse/fallback and mode switch verified"
exit 0
