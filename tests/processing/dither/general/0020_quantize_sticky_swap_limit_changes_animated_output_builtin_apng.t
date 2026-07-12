#!/bin/sh
# TAP test ensuring sticky swap_limit admits capped palette changes.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/apng_8x8_rgba_loop2.png"
locked_output=''
capped_output=''

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -Qsticky -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v
set +xv

locked_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qsticky:swap_limit=0:scene_cut_threshold=1.0 \
        -d fs -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "sticky swap_limit=0 encode failed"
    exit 0
}

capped_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qsticky:swap_limit=2:scene_cut_threshold=1.0 \
        -d fs -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "sticky swap_limit=2 encode failed"
    exit 0
}

set -xv

test "${locked_output}" != "${capped_output}" || {
    echo "not ok" 1 - "sticky swap_limit did not change animated output"
    exit 0
}

echo "ok" 1 - "sticky swap_limit changes animated output below scene cut"
exit 0
