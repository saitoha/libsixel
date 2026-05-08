#!/bin/sh
# Verify built-in palette GPL export remains stable.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
expected_palette='GIMP Palette
Name: libsixel export
Columns: 16
# Exported by libsixel
  0   0   0	Index 0
128   0   0	Index 1
  0 128   0	Index 2
128 128   0	Index 3
  0   0 128	Index 4
128   0 128	Index 5
  0 128 128	Index 6
192 192 192	Index 7
128 128 128	Index 8
255   0   0	Index 9
  0 255   0	Index 10
255 255   0	Index 11
  0   0 255	Index 12
255   0 255	Index 13
  0 255 255	Index 14
255 255 255	Index 15'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -bxterm16 -M gpl:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "builtin palette GPL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "builtin GPL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "builtin palette GPL output changed"
    exit 0
}

echo "ok" 1 - "built-in palette GPL output is stable"
exit 0
