#!/bin/sh
# a_dither accepts and ignores perturbation
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit -d a_dither "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit -d a_dither:perturb=0.5:perturb_seed=2 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
test "${first}" = "${second}" || {
    echo "not ok 1 - a_dither must ignore perturbation"
    exit 0
}

echo "ok 1 - a_dither accepts and ignores perturbation"
exit 0
