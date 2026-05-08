#!/bin/sh
# Verify VT340 color built-in palette PAL export remains stable.
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
51 51 204
204 33 33
51 204 51
204 51 204
51 204 204
204 204 51
135 135 135
66 66 66
84 84 153
153 66 66
84 153 84
153 84 153
84 153 153
153 153 84
204 204 204
0 0 0'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bvt340color -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "VT340 color palette PAL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "VT340 color PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "VT340 color palette PAL output changed"
    exit 0
}

echo "ok" 1 - "VT340 color palette PAL output is stable"
exit 0
