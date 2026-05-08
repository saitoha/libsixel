#!/bin/sh
# Verify high-color mode does not change encoded output with mapfile palettes.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_palette="${TOP_SRCDIR}/images/map64.six"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -m "${mapfile_palette}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "mapfile palette baseline encode failed"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -I -m "${mapfile_palette}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "mapfile palette encode failed with -I"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "mapfile palette output changed under -I"
    exit 0
}

echo "ok" 1 - "mapfile palette output ignores high-color mode"
exit 0
