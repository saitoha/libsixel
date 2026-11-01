#!/bin/sh
# Verify gray4 built-in palette PAL export remains stable.
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
0 0 0
17 17 17
34 34 34
51 51 51
68 68 68
85 85 85
102 102 102
119 119 119
136 136 136
153 153 153
170 170 170
187 187 187
204 204 204
221 221 221
238 238 238
255 255 255'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bgray4 -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "gray4 palette PAL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "gray4 PAL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "gray4 palette PAL output changed"
    exit 0
}

echo "ok" 1 - "gray4 palette PAL output is stable"
exit 0
