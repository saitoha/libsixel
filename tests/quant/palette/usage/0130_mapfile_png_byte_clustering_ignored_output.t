#!/bin/sh
# Verify legacy byte PNG map construction ignores clustering colorspace.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

# Pin the byte path: a CMS-retained float map uses native construction
# controls and may reorder entries when the clustering space changes.

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/images/map8-palette.png"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision=8bit -Wgamma -0hard \
        -m "${mapfile_png}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "byte PNG mapfile baseline encode failed"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision=8bit -Wgamma -0hard \
        -X oklab -m "${mapfile_png}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "byte PNG mapfile encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "byte PNG mapfile output changed under -X oklab"
    exit 0
}

echo "ok" 1 - "byte PNG mapfile output ignores clustering colorspace"
exit 0
