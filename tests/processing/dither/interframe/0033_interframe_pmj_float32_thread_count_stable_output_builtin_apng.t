#!/bin/sh
# TAP test ensuring float32 PMJ output is stable across thread counts.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${SIXEL_ENABLE_THREADS-0}" = 1 || {
    printf "1..0 # SKIP thread backend is unavailable\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

single_thread_output=$(
    SIXEL_DITHER_STBN_SOURCE=pmj \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -S -T 1 \
        -d interframe -p 2 \
        "${input_apng}"
) || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

multi_thread_output=$(
    SIXEL_DITHER_STBN_SOURCE=pmj \
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=2 \
        --precision=float32 \
        -L builtin \
        -ldisable \
        -S -T 1 \
        -d interframe -p 2 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "float32 pmj multi-thread encode failed"
    exit 0
}

test "${multi_thread_output}" = "${single_thread_output}" || {
    echo "not ok" 1 - "float32 pmj output changed across thread counts"
    exit 0
}

echo "ok" 1 - "float32 pmj output is stable across thread counts"
exit 0
