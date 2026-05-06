#!/bin/sh
# Verify -X does not change encoded output with PAL mapfiles.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -m pal:- -o - "${input_image}" <<'PAL'
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
    echo "not ok" 1 - "PAL mapfile baseline encode failed"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -X oklab -m pal:- -o - "${input_image}" <<'PAL'
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
    echo "not ok" 1 - "PAL mapfile encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "PAL mapfile output changed under -X oklab"
    exit 0
}

echo "ok" 1 - "PAL mapfile output ignores clustering colorspace"
exit 0
