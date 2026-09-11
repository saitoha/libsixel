#!/bin/sh
# Verify JASC PAL export writes the complete canonical text layout.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
expected_palette='JASC-PAL
0100
2
0 0 0
255 255 255'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -bgray1 \
        -M pal-jasc:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "JASC PAL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "JASC PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "JASC PAL layout changed"
    exit 0
}

echo "ok" 1 - "JASC PAL export has exact header, count, and RGB rows"
exit 0
