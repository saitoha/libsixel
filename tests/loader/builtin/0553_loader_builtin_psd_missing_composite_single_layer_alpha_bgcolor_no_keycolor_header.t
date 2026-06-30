#!/bin/sh
# Verify missing-composite single-layer RGB8+alpha disables keycolor with --bgcolor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_rgb8_alpha_missing_composite_single_layer.psd"
keycolor_header="$(printf '\033P0;0q')"
output_bg=''
status_bg=0

output_bg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
    -d fs:scan=raster -B "#000000" "${input_psd}" 2>&1) || status_bg=$?

test "${status_bg}" -eq 0 || {
    echo "not ok 1 - missing-composite single-layer RGB8+alpha render failed with --bgcolor"
    exit 0
}

test "${output_bg#*"${keycolor_header}"}" = "${output_bg}" || {
    echo "not ok 1 - missing-composite single-layer RGB8+alpha kept keycolor header with --bgcolor"
    exit 0
}

echo "ok 1 - missing-composite single-layer RGB8+alpha suppresses keycolor header with --bgcolor"
exit 0
