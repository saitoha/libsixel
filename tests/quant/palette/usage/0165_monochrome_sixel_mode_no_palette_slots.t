#!/bin/sh
# Policy: docs/encoder/palette-construction/fixed-palettes.md
# Verify monochrome SIXEL mode does not emit palette slot definitions.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

sixel_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -A composite -L builtin -e -o - "${input_image}") || {
    echo "not ok" 1 - "monochrome SIXEL mode emitted palette slots"
    exit 0
}

case "${sixel_output}" in *P0\;1q*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "monochrome SIXEL mode emitted palette slots"
    exit 0
}
case "${sixel_output}" in *#*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "monochrome SIXEL mode emitted palette slots"
    exit 0
}

echo "ok" 1 - "monochrome SIXEL mode omits palette slots"
exit 0
