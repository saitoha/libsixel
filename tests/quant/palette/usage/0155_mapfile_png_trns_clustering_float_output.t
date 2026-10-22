#!/bin/sh
# Verify required clustering precision for a PNG tRNS image map.
# Policy: docs/encoder/palette-construction/external-palettes.md
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
mapfile_png="${TOP_SRCDIR}/tests/data/inputs/formats/snake-png-pal8-trns.png"

expected_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision-policy=float -Xoklab \
        -Wgamma -Fnone -_none -m "${mapfile_png}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "PNG tRNS mapfile baseline encode failed"
    exit 0
}

test -n "${expected_output}" || {
    echo "not ok" 1 - "PNG tRNS mapfile output is empty"
    exit 0
}

actual_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Lbuiltin:cms_engine=none! --precision-policy=auto -Xoklab \
        -Wgamma -Fnone -_none -m "${mapfile_png}" -o - "${input_image}"
) || {
    echo "not ok" 1 - "PNG tRNS mapfile encode failed with -X oklab"
    exit 0
}

test "${actual_output}" = "${expected_output}" || {
    echo "not ok" 1 - "PNG tRNS mapfile lost float clustering precision"
    exit 0
}

echo "ok" 1 - "PNG tRNS mapfile retains required clustering precision"
exit 0
