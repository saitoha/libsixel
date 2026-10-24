#!/bin/sh
# Verify clustering colorspace does not promote explicit byte working data.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled in this build"
    exit 0
}

echo "1..1"
set -v

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
planner_log=$(set +xv; SIXEL_PRECISION_POLICY=auto \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin:cms_engine=none! --threading-policy=balanced:global_budget=1 \
    --precision-policy=8bit -Xoklab -Qkmeans -dnone -p16 \
    -v -o/dev/null "${input_image}" 2>&1) || {
    echo "not ok 1 - clustering colorspace conversion failed"
    exit 0
}
# An unmanaged byte input keeps byte work; clustering owns its float view.
case "${planner_log}" in *work=rgb888**) true ;; *) false ;; esac || {
    echo "not ok 1 - clustering colorspace promoted the main pipeline"
    exit 0
}

echo "ok 1 - clustering colorspace keeps 8-bit main work"
exit 0
