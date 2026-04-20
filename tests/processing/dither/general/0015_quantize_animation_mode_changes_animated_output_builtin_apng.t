#!/bin/sh
# TAP test ensuring -Q animation_mode changes animated APNG output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

input_apng="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_apng_12x8_rgba_loop2.png"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 \
    -L builtin \
    -ldisable \
    -S -T 1 \
    -Qauto -d fs -p 2 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

baseline_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto -d fs -p 2 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "animated APNG baseline encode failed"
    exit 0
}

animation_mode_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:animation_mode=1 -d fs -p 2 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "animated APNG animation_mode encode failed"
    exit 0
}

test "${baseline_output}" != "${animation_mode_output}" || {
    echo "not ok" 1 - "animation_mode did not change animated APNG output"
    exit 0
}

echo "ok" 1 - "-Q animation_mode changes animated APNG output"
exit 0
