#!/bin/sh
# Verify automatic precision preserves required PNG map clustering precision.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Compare required map-only promotion against the existing explicit float
# path; independent color values are asserted by the map_cluster tests.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision-policy=float -Xoklab -Wgamma -0hard \
        -m "${mapfile_png}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "explicit float PNG mapfile baseline encode failed"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision-policy=auto -Wgamma -0hard \
        -X oklab -m "${mapfile_png}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "automatic PNG mapfile encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "PNG mapfile lost required float clustering precision"
    exit 0
}

echo "ok" 1 - "PNG mapfile clustering keeps float precision automatically"
exit 0
