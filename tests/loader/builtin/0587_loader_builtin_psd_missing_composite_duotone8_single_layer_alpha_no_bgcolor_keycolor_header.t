#!/bin/sh
# Verify missing-composite single-layer Duotone8+alpha keeps keycolor without --bgcolor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_duotone8_alpha_missing_composite_single_layer.psd"
keycolor_header="$(printf '\033P0;0q')"
output_no=''
status_no=0

output_no=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
    -d fs:scan=raster "${input_psd}" 2>&1) || status_no=$?

test "${status_no}" -eq 0 || {
    echo "not ok 1 - missing-composite single-layer Duotone8+alpha render failed without --bgcolor"
    exit 0
}

test "${output_no#*"${keycolor_header}"}" != "${output_no}" || {
    echo "not ok 1 - missing-composite single-layer Duotone8+alpha did not emit keycolor header without --bgcolor"
    exit 0
}

echo "ok 1 - missing-composite single-layer Duotone8+alpha emits keycolor header without --bgcolor"
exit 0
