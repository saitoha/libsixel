#!/bin/sh
# Verify precision mode does not reinterpret PAL mapfiles.
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
8
162 6 6
6 178 6
150 158 6
126 106 250
194 6 182
6 174 186
194 194 194
2 2 2'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --precision=float32 -m pal:- -M pal:- -o /dev/null \
        "${input_image}" <<'PAL'
JASC-PAL
0100
8
162 6 6
6 178 6
150 158 6
126 106 250
194 6 182
6 174 186
194 194 194
2 2 2
PAL
) || {
    echo "not ok" 1 - "PAL mapfile export failed with float32 precision"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "PAL mapfile changed under float32 precision"
    exit 0
}

echo "ok" 1 - "PAL mapfile is stable under precision mode"
exit 0
