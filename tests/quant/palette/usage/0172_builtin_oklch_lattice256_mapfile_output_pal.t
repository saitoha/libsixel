#!/bin/sh
# Verify OKLCh lattice built-in palette PAL export remains stable.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
expected_prefix='JASC-PAL
0100
256
0 0 0
1 1 1
2 2 2
3 3 3
7 7 7'
expected_middle='255 255 255
34 15 21
45 3 21'
expected_suffix='214 159 198
247 132 218
233 206 224
251 196 234'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -boklch-lattice256 -M pal:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "OKLCh lattice palette PAL export failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "OKLCh lattice PAL output normalization failed"
    exit 0
}

actual_without_prefix=${actual_palette#"${expected_prefix}"}
test "${actual_without_prefix}" != "${actual_palette}" || {
    echo "not ok" 1 - "OKLCh lattice PAL header or gray ramp changed"
    exit 0
}

actual_without_middle=${actual_palette#*"${expected_middle}"}
test "${actual_without_middle}" != "${actual_palette}" || {
    echo "not ok" 1 - "OKLCh lattice chroma ramp start changed"
    exit 0
}

actual_without_suffix=${actual_palette%"${expected_suffix}"}
test "${actual_without_suffix}" != "${actual_palette}" || {
    echo "not ok" 1 - "OKLCh lattice PAL tail changed"
    exit 0
}

echo "ok" 1 - "OKLCh lattice palette PAL output is stable"
exit 0
