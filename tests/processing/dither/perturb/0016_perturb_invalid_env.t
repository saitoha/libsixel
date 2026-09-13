#!/bin/sh
# invalid environment uses defaults
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=1 --gpu-policy=off -p 16 --precision=8bit --env SIXEL_DITHER_PERTURB=1.5 --env SIXEL_DITHER_PERTURB_SEED=2147483648 -d fs "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - conversion failed"
    exit 0
}
test "${first}" = "${second}" || {
    echo "not ok 1 - invalid environment must use defaults"
    exit 0
}

echo "ok 1 - invalid environment uses defaults"
exit 0
