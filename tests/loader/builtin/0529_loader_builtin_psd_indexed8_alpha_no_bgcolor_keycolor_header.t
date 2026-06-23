#!/bin/sh
# Verify builtin PSD Indexed8 alpha keeps keycolor without --bgcolor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_indexed8_alpha.psd"
keycolor_header="$(printf '\033P0;0q')"
output_no=''
output_bg=''
status_no=0
status_bg=0

output_no=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
    -d fs:scan=raster "${input_psd}" 2>&1) || status_no=$?

test "${status_no}" -eq 0 || {
    echo "not ok 1 - builtin PSD Indexed8 alpha render failed without --bgcolor"
    exit 0
}

test "${output_no#*"${keycolor_header}"}" != "${output_no}" || {
    echo "not ok 1 - builtin PSD Indexed8 alpha did not emit keycolor header without --bgcolor"
    exit 0
}

output_bg=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin! \
    -d fs:scan=raster -B "#000000" "${input_psd}" 2>&1) || status_bg=$?

test "${status_bg}" -eq 0 || {
    echo "not ok 1 - builtin PSD Indexed8 alpha render failed with --bgcolor"
    exit 0
}

test "${output_bg#*"${keycolor_header}"}" = "${output_bg}" || {
    echo "not ok 1 - builtin PSD Indexed8 alpha kept keycolor header with --bgcolor"
    exit 0
}

echo "ok 1 - builtin PSD Indexed8 alpha keycolor header toggles with --bgcolor"
exit 0
