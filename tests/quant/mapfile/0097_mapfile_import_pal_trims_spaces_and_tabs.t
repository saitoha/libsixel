#!/bin/sh
# Verify JASC PAL import trims leading and trailing spaces and tabs.
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
input_palette="${TOP_SRCDIR}/tests/data/inputs/mapfile/pal-trim-whitespace.pal"
expected_palette='JASC-PAL
0100
2
12 34 56
200 210 220'

actual_palette=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -m pal-jasc:"${input_palette}" \
        -M pal-jasc:- -o /dev/null "${input_image}") || {
    echo "not ok" 1 - "JASC PAL surrounding whitespace was rejected"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "JASC PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "JASC PAL whitespace changed imported entries"
    exit 0
}

echo "ok" 1 - "JASC PAL surrounding spaces and tabs are trimmed"
exit 0
