#!/bin/sh
# Verify JASC PAL import accepts CRLF line endings.
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
12 34 56
200 210 220'

actual_palette=$(
    printf 'JASC-PAL\r\n0100\r\n2\r\n12 34 56\r\n200 210 220\r\n' | \
        ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m pal-jasc:- \
            -M pal-jasc:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "CRLF JASC PAL import failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "CRLF JASC PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "CRLF JASC PAL entries changed"
    exit 0
}

echo "ok" 1 - "JASC PAL import accepts CRLF line endings"
exit 0
