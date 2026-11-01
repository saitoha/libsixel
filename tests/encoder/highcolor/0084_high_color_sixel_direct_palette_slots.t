#!/bin/sh
# Verify high-color SIXEL direct mode keeps its 255-slot palette boundary.
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
    -L builtin -I -o - "${input_image}") || {
    echo "not ok" 1 - "high-color SIXEL direct palette slots changed"
    exit 0
}

case "${sixel_output}" in *#0\;2\;100\;0\;0*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "high-color SIXEL direct palette slots changed"
    exit 0
}
case "${sixel_output}" in *#5\;2\;100\;0\;100*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "high-color SIXEL direct palette slots changed"
    exit 0
}
case "${sixel_output}" in *#254\;2\;0\;0\;0*) true ;; *) false ;; esac || {
    echo "not ok" 1 - "high-color SIXEL direct palette slots changed"
    exit 0
}
case "${sixel_output}" in *#255\;2\;*) false ;; *) true ;; esac || {
    echo "not ok" 1 - "high-color SIXEL direct palette slots changed"
    exit 0
}

echo "ok" 1 - "high-color SIXEL direct mode defines slots 0 through 254"
exit 0
