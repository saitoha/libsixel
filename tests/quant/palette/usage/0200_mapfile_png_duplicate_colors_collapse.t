#!/bin/sh
# Verify duplicate decoded indexed-PNG colors collapse into one map entry.
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
mapfile_png="${TOP_SRCDIR}/images/pngsuite/transparency/tm3n3p02.png"
expected_palette='JASC-PAL
0100
1
2 2 255'

# All four PLTE entries are blue.  Their alpha values differ, so compositing
# them over the same blue background makes every decoded pixel identical.
actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --alpha-policy=composite \
        -B '#0000ff' -m "${mapfile_png}" -M pal-jasc:- \
        -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "duplicate-color indexed PNG mapfile export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "duplicate-color PNG palette normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "duplicate decoded PNG colors did not collapse"
    exit 0
}

echo "ok" 1 - "duplicate decoded indexed-PNG colors collapse"
exit 0
