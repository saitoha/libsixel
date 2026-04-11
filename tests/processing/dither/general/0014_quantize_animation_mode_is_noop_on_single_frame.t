#!/bin/sh
# TAP test ensuring -Q animation_mode is a no-op on single-frame input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_ppm="${TOP_SRCDIR}/tests/data/inputs/small.ppm"

echo "1..1"
set -v

baseline_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Qauto -d fs -p 16 \
        "${input_ppm}"
) || {
    echo "not ok" 1 - "single-frame baseline encode failed"
    exit 0
}

animation_mode_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        -Qauto:animation_mode=1 -d fs -p 16 \
        "${input_ppm}"
) || {
    echo "not ok" 1 - "single-frame animation_mode encode failed"
    exit 0
}

test "${baseline_output}" = "${animation_mode_output}" || {
    echo "not ok" 1 - "animation_mode changed single-frame output"
    exit 0
}

echo "ok" 1 - "-Q animation_mode is a no-op on single-frame input"
exit 0
