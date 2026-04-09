#!/bin/sh
# TAP test ensuring temporal diffusion state resets before size-changing input.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_WEBP-}" = 1 || {
    printf "1..0 # SKIP webp loader is unavailable\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_anim="${TOP_SRCDIR}/tests/data/inputs/formats/orientation_plain_anim_12x8.webp"
input_gif="${TOP_SRCDIR}/tests/data/inputs/snake_64.gif"
combined_sixel="${ARTIFACT_LOCAL_DIR}/temporal-size-change-combined.six"
single_sixel="${ARTIFACT_LOCAL_DIR}/temporal-size-change-single.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -ldisable \
    -d temporal-diffusion -p 16 \
    "${input_anim}" "${input_gif}" >"${combined_sixel}" || {
    echo "not ok" 1 - "temporal-diffusion combined encode failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -ldisable \
    -d temporal-diffusion -p 16 \
    "${input_gif}" >"${single_sixel}" || {
    echo "not ok" 1 - "temporal-diffusion single encode failed"
    exit 0
}

frame_count=$(awk 'BEGIN{RS="\033\\\\"} END{print NR}' "${combined_sixel}")
test "${frame_count}" -ge 3 || {
    echo "not ok" 1 - "combined encode did not emit expected frame count"
    exit 0
}

combined_frame_3=$(awk 'BEGIN{RS="\033\\\\"} NR==3{printf "%s",$0}' \
    "${combined_sixel}")
single_frame_1=$(awk 'BEGIN{RS="\033\\\\"} NR==1{printf "%s",$0}' \
    "${single_sixel}")

test "${combined_frame_3}" = "${single_frame_1}" || {
    echo "not ok" 1 - "size change carried temporal state into next input"
    exit 0
}

echo "ok" 1 - "temporal-diffusion resets across size change"
exit 0
