#!/bin/sh
# Verify PAL mapfile GPL export remains stable.
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
255 255 255	Index 1'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -m pal:- -M gpl:- -o /dev/null "${input_image}" <<'PAL'
JASC-PAL
0100
2
0 0 0
255 255 255
PAL
) || {
    echo "not ok" 1 - "PAL mapfile GPL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "PAL mapfile GPL output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "PAL mapfile GPL output changed"
    exit 0
}

echo "ok" 1 - "PAL mapfile GPL output is stable"
exit 0
