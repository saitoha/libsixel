#!/bin/sh
# Verify xterm16 built-in SIXEL output keeps the expected slot range.
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
    -L builtin -b xterm16 -o - "${input_image}") || {
    echo "not ok" 1 - "xterm16 SIXEL palette slot range changed"
    exit 0
}

test "${sixel_output#*#0;2;0;0;0}" != "${sixel_output}" || {
    echo "not ok" 1 - "xterm16 SIXEL palette slot range changed"
    exit 0
}
test "${sixel_output#*#15;2;100;100;100}" != "${sixel_output}" || {
    echo "not ok" 1 - "xterm16 SIXEL palette slot range changed"
    exit 0
}
test "${sixel_output#*#16;2;}" = "${sixel_output}" || {
    echo "not ok" 1 - "xterm16 SIXEL palette slot range changed"
    exit 0
}

echo "ok" 1 - "xterm16 SIXEL output defines slots 0 through 15"
exit 0
