#!/bin/sh
# Verify mapfile SIXEL output keeps the source palette slot range.
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x

input_image="${TOP_SRCDIR}/tests/data/inputs/small.ppm"
mapfile="${TOP_SRCDIR}/images/map8.six"

sixel_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -L builtin -m "${mapfile}" -o - "${input_image}") || {
    echo "not ok" 1 - "mapfile SIXEL palette slot range changed"
    exit 0
}

test "${sixel_output#*#0;2;60;1;1}" != "${sixel_output}" || {
    echo "not ok" 1 - "mapfile SIXEL palette slot range changed"
    exit 0
}
test "${sixel_output#*#7;2;1;1;1}" != "${sixel_output}" || {
    echo "not ok" 1 - "mapfile SIXEL palette slot range changed"
    exit 0
}
test "${sixel_output#*#8;2;}" = "${sixel_output}" || {
    echo "not ok" 1 - "mapfile SIXEL palette slot range changed"
    exit 0
}

echo "ok" 1 - "mapfile SIXEL output defines slots 0 through 7"
exit 0
