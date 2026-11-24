#!/bin/sh
# different seeds change 8bit output
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=greedy:global_budget=1 --gpu-policy=off -p 16 --precision-policy=8bit -d fs:perturb=0.5:perturb_seed=1 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    printf '\n%s\n' "not ok 1 - conversion failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threading-policy=greedy:global_budget=1 --gpu-policy=off -p 16 --precision-policy=8bit -d fs:perturb=0.5:perturb_seed=2 "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    printf '\n%s\n' "not ok 1 - conversion failed"
    exit 0
}
test "${first}" != "${second}" || {
    printf '\n%s\n' "not ok 1 - seeds must change output"
    exit 0
}

printf '\n%s\n' "ok 1 - different seeds change 8bit output"
exit 0
