#!/bin/sh
# Verify lookup policy does not change built-in palette export.
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
128 0 0
0 128 0
128 128 0
0 0 128
128 0 128
0 128 128
192 192 192
128 128 128
255 0 0
0 255 0
255 255 0
0 0 255
255 0 255
0 255 255
255 255 255'

actual_none=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bxterm16 -~ none -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette export failed with -~ none"
    exit 0
}
actual_none=$(printf "%s" "${actual_none}" | tr -d '\015') || {
    echo "not ok" 1 - "builtin none palette output normalization failed"
    exit 0
}

actual_eytzinger=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bxterm16 -~ eytzinger -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette export failed with -~ eytzinger"
    exit 0
}
actual_eytzinger=$(printf "%s" "${actual_eytzinger}" | tr -d '\015') || {
    echo "not ok" 1 - "builtin eytzinger palette output normalization failed"
    exit 0
}

actual_fhedt=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bxterm16 -~ fhedt -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette export failed with -~ fhedt"
    exit 0
}
actual_fhedt=$(printf "%s" "${actual_fhedt}" | tr -d '\015') || {
    echo "not ok" 1 - "builtin fhedt palette output normalization failed"
    exit 0
}

test "${actual_none}" = "${expected_palette}" || {
    echo "not ok" 1 - "builtin palette changed under -~ none"
    exit 0
}
test "${actual_eytzinger}" = "${expected_palette}" || {
    echo "not ok" 1 - "builtin palette changed under -~ eytzinger"
    exit 0
}
test "${actual_fhedt}" = "${expected_palette}" || {
    echo "not ok" 1 - "builtin palette changed under -~ fhedt"
    exit 0
}

echo "ok" 1 - "built-in palette export is stable under lookup policies"
exit 0
