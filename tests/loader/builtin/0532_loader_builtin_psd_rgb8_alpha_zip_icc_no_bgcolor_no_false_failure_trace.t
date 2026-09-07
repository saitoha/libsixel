#!/bin/sh
# Verify builtin PSD RGB8 alpha ZIP ICC keeps keycolor and avoids false failure trace without --bgcolor.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_rgb8_alpha_zip_icc.psd"
keycolor_header="$(printf '\033P0;1q')"
output_text=''
status_code=0

output_text=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! \
    -d fs:scan=raster "${input_psd}" 2>&1) || status_code=$?

test "${status_code}" -eq 0 || {
    echo "not ok 1 - builtin PSD RGB8 alpha ZIP ICC render failed without --bgcolor"
    exit 0
}

test "${output_text#*"${keycolor_header}"}" != "${output_text}" || {
    echo "not ok 1 - builtin PSD RGB8 alpha ZIP ICC did not emit keycolor header without --bgcolor"
    exit 0
}

test "${output_text#*embedded ICC conversion failed}" = "${output_text}" || {
    echo "not ok 1 - builtin PSD RGB8 alpha ZIP ICC emitted false failure trace without --bgcolor"
    exit 0
}

echo "ok 1 - builtin PSD RGB8 alpha ZIP ICC keeps keycolor and has no false failure trace"
exit 0
