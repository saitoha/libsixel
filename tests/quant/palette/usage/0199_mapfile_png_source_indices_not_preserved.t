#!/bin/sh
# Verify an indexed PNG mapfile rebuilds entries from decoded pixel order.
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
mapfile_png="${ARTIFACT_LOCAL_DIR}/index-gap.png"
expected_palette='JASC-PAL
0100
2
255 255 255
2 2 2'

# This 2x1 indexed PNG defines black, red, and white at source indices 0, 1,
# and 2, then stores pixels 2 and 0.  Index 1 is deliberately unused.
printf '\211\120\116\107\015\012\032\012\000\000\000\015\111\110\104\122\000\000\000\002\000\000\000\001\010\003\000\000\000\303\374\217\270\000\000\000\011\120\114\124\105\000\000\000\377\000\000\377\377\377\147\031\144\036\000\000\000\013\111\104\101\124\170\332\143\140\142\000\000\000\007\000\003\125\221\225\135\000\000\000\000\111\105\116\104\256\102\140\202' \
    >"${mapfile_png}"

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -m "${mapfile_png}" \
        -M pal-jasc:- -o /dev/null "${input_image}"
) || {
    echo "not ok" 1 - "indexed PNG with a source index gap failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "indexed PNG palette normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "indexed PNG source indices were retained"
    exit 0
}

echo "ok" 1 - "indexed PNG source indices are not preserved"
exit 0
