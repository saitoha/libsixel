#!/bin/sh
# Verify VT340 monochrome built-in palette PAL export remains stable.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
expected_palette='JASC-PAL
0100
16
33 33 33
66 66 66
102 102 102
15 15 15
51 51 51
84 84 84
117 117 117
0 0 0
33 33 33
66 66 66
102 102 102
15 15 15
51 51 51
84 84 84
117 117 117
0 0 0'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bvt340mono -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "VT340 monochrome palette PAL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "VT340 monochrome PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "VT340 monochrome palette PAL output changed"
    exit 0
}

echo "ok" 1 - "VT340 monochrome palette PAL output is stable"
exit 0
