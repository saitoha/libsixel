#!/bin/sh
# TAP test ensuring scene_cut_threshold changes animated APNG output.

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
    -Qauto -d fs -p 16 \
    "${input_apng}" >/dev/null 2>&1 || {
    printf "1..0 # SKIP animated builtin APNG frame path is unavailable\n"
    exit 0
}

echo "1..1"
set -v

low_threshold_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:animation_mode=1:scene_cut_threshold=0.00 \
        -d fs -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "scene_cut_threshold=0.00 encode failed"
    exit 0
}

high_threshold_output=$(
    ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --threads=1 \
        -L builtin \
        -ldisable \
        -Qauto:animation_mode=1:scene_cut_threshold=1.00 \
        -d fs -p 16 \
        "${input_apng}"
) || {
    echo "not ok" 1 - "scene_cut_threshold=1.00 encode failed"
    exit 0
}

test "${low_threshold_output}" != "${high_threshold_output}" || {
    echo "not ok" 1 - "scene_cut_threshold did not change animated APNG output"
    exit 0
}

echo "ok" 1 - "-Q scene_cut_threshold changes animated APNG output"
exit 0
