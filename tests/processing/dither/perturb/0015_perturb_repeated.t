#!/bin/sh
# repeated diffusion resets perturbation
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit --env SIXEL_DITHER_PERTURB=0 -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit --env SIXEL_DITHER_PERTURB=0 -d fs:perturb=0.5:perturb_seed=2 -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
test "${first}" = "${second}" || {
    echo "not ok 1 - repeated diffusion must reset settings"
    exit 0
}

echo "ok 1 - repeated diffusion resets perturbation"
exit 0
