#!/bin/sh
# TAP test ensuring temporal diffusion state is reset between input files.

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
output_sixel="${ARTIFACT_LOCAL_DIR}/temporal-reset-between-inputs.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --threads=1 -ldisable \
    -d temporal-diffusion -p 16 \
    "${input_anim}" "${input_anim}" >"${output_sixel}" || {
    echo "not ok" 1 - "temporal-diffusion two-input encode failed"
    exit 0
}

frame_count=$(awk 'BEGIN{RS="\033\\\\"} END{print NR}' "${output_sixel}")
test "${frame_count}" -ge 4 || {
    echo "not ok" 1 - "two-input encode did not emit four frames"
    exit 0
}

frame_1=$(awk 'BEGIN{RS="\033\\\\"} NR==1{printf "%s",$0}' "${output_sixel}")
frame_2=$(awk 'BEGIN{RS="\033\\\\"} NR==2{printf "%s",$0}' "${output_sixel}")
frame_3=$(awk 'BEGIN{RS="\033\\\\"} NR==3{printf "%s",$0}' "${output_sixel}")
frame_4=$(awk 'BEGIN{RS="\033\\\\"} NR==4{printf "%s",$0}' "${output_sixel}")

test "${frame_1}" = "${frame_3}" || {
    echo "not ok" 1 - "temporal state leaked into second input frame 0"
    exit 0
}

test "${frame_2}" = "${frame_4}" || {
    echo "not ok" 1 - "temporal state leaked into second input frame 1"
    exit 0
}

echo "ok" 1 - "temporal-diffusion resets between input files"
exit 0
