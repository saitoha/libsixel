#!/bin/sh
# fixed bands are deterministic across worker counts
set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    echo "1..0 # SKIP img2sixel is disabled"
    exit 0
}

echo "1..1"
set -v

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    echo "ok 1 # SKIP threading is disabled"
    exit 0
}

# Hold band history fixed while changing scheduling and worker count.
first=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=8 \
    --gpu-policy=off --precision=8bit -p 16 \
    -d fs:perturb=0.5:scan=serpentine:band_width=12:threads_max=2 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - parallel conversion failed"
    exit 0
}
second=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=8 \
    --gpu-policy=off --precision=8bit -p 16 \
    -d fs:perturb=0.5:scan=serpentine:band_width=12:threads_max=3 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - parallel conversion failed"
    exit 0
}
third=$(${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --threads=8 \
    --gpu-policy=off --precision=8bit -p 16 \
    -d fs:perturb=0.5:scan=serpentine:band_width=12:threads_max=4 \
    "${TOP_SRCDIR}/tests/data/inputs/snake_64.png") || {
    echo "not ok 1 - parallel conversion failed"
    exit 0
}
test "${first}" = "${second}" || {
    echo "not ok 1 - parallel output differs"
    exit 0
}
test "${first}" = "${third}" || {
    echo "not ok 1 - parallel output differs"
    exit 0
}

echo "ok 1 - fixed bands are deterministic across worker counts"
exit 0
