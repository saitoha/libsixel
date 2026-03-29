#!/bin/sh
# Verify builtin GIF transparency + disposal=3 frame composition is stable.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}


echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_gif="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3.gif"
reference_png="${TOP_SRCDIR}/tests/data/inputs/formats/gif-transparent-anim-dispose3-frame2-reference.png"
output_six="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_dispose3_frame2.six"
normalized_six="${ARTIFACT_LOCAL_DIR}/builtin_gif_transparent_dispose3_frame2_norm.six"
lsqa_floor=0.995

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --env SIXEL_THREADS=4 \
              --env SIXEL_LOADER_ANIMATION_START_FRAME_NO=2 \
              -Lbuiltin! \
              -ldisable -d none -p 256 -y raster -g \
              "${input_gif}" >"${output_six}" || {
    echo "not ok" 1 - "builtin transparent dispose3 frame2 decode failed"
    exit 0
}

prefix_hex="$(od -An -t x1 -N 3 "${output_six}" \
    | LC_ALL=C tr -d '[:space:]' \
    | LC_ALL=C tr 'A-F' 'a-f')"
test "${prefix_hex}" = "1b5b48" || {
    cp "${output_six}" "${normalized_six}" || {
        echo "not ok" 1 - "builtin transparent dispose3 frame2 normalization copy failed"
        exit 0
    }
}

test "${prefix_hex}" != "1b5b48" || {
    dd if="${output_six}" of="${normalized_six}" bs=1 skip=3 2>/dev/null || {
        echo "not ok" 1 - "builtin transparent dispose3 frame2 normalization failed"
        exit 0
    }
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -b "MS-SSIM:${lsqa_floor}" \
    "${reference_png}" "${normalized_six}" 2>&1) || {
    echo "not ok" 1 - "${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "builtin GIF transparent dispose3 frame2 matches reference"

exit 0
