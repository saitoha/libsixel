#!/bin/sh
# Verify alpha-policy strict parsing and invalid fallback behavior.

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
    echo "not ok 1 - default alpha policy render failed"
    exit 0
}

out_composite=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_ALPHA_POLICY=composite \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - composite alpha policy render failed"
    exit 0
}

out_invalid=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_ALPHA_POLICY=invalid \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - invalid alpha policy render failed"
    exit 0
}

out_keep=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_ALPHA_POLICY=keep \
    -Lbuiltin:cms_engine=none! -B#fff -d fs:scan=raster "${input_bmp}") || {
    echo "not ok 1 - keep alpha policy render failed"
    exit 0
}

test "${out_default}" = "${out_composite}" || {
    echo "not ok 1 - default policy mismatch against explicit composite"
    exit 0
}

test "${out_default}" = "${out_invalid}" || {
    echo "not ok 1 - invalid alpha policy did not fall back to default"
    exit 0
}

test "${out_default}" != "${out_keep}" || {
    echo "not ok 1 - keep policy did not change alpha-zero behavior"
    exit 0
}

echo "ok 1 - alpha policy strict parse/fallback and mode switch verified"
exit 0
