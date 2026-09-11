#!/bin/sh
# Verify an animated GIF mapfile uses one static first-frame palette source.
# Test-plan: docs/testing/mapfile-parser-coverage.md
# Policy: docs/functionality/external-palettes.md

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
mapfile_image="${TOP_SRCDIR}/tests/data/inputs/formats/gif-anim-no-netscape-2frame.gif"
expected_palette='JASC-PAL
0100
1
255 2 2'

actual_palette=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! \
        --gpu-policy=off -Wgamma -Ugamma -m "${mapfile_image}" \
        -M pal-jasc:- -o/dev/null "${input_image}"
) || {
    echo "not ok" 1 - "animated GIF image mapfile conversion failed"
    exit 0
}
actual_palette=$(printf "%s" "${actual_palette}" | tr -d '\015') || {
    echo "not ok" 1 - "animated GIF mapfile output normalization failed"
    exit 0
}

test "${actual_palette}" = "${expected_palette}" || {
    echo "not ok" 1 - "animated GIF mapfile consumed more than its first frame"
    exit 0
}

echo "ok" 1 - "animated GIF mapfile uses one static first-frame source"
exit 0
