#!/bin/sh
# Verify ICC PNG mapfile colorspace conversion remains deterministic.
# Policy: docs/functionality/external-palettes.md
# Policy: docs/functionality/output-colorspace.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# CMS is disabled. The existing quantizer produces byte-linear entries
# (90,2,2), (2,110,2), etc. Apply the sRGB transfer curve and round to
# RGB8 for export, independently of -Ulinear; e.g. 90/255 -> 160/255.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/tests/data/inputs/formats/map8_embedded_icc.png"
expected_palette='JASC-PAL
0100
8
160 22 22
22 175 22
150 160 22
127 108 249
192 22 181
22 173 187
192 192 192
22 22 22'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --cms-engine=none \
        -L builtin! --working-colorspace=linear --output-colorspace=linear \
        -m "${mapfile_png}" -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "ICC PNG mapfile export failed with -W/-U linear"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "ICC PNG mapfile output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "ICC PNG mapfile converted palette changed under -W/-U linear"
    exit 0
}

echo "ok" 1 - "ICC PNG mapfile colorspace conversion is deterministic"
exit 0
