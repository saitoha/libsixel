#!/bin/sh
# explicit perturbation overrides environment
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit -d fs:perturb=0.25:perturb_seed=1 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit --env SIXEL_DITHER_PERTURB=1 --env SIXEL_DITHER_PERTURB_SEED=2 -d fs:perturb=0.25:perturb_seed=1 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
test "${first}" = "${second}" || {
    echo "not ok 1 - CLI must win over environment"
    exit 0
}

echo "ok 1 - explicit perturbation overrides environment"
exit 0
