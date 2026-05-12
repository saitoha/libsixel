#!/bin/sh
# Run lsqa quality checks for k-means soft histogram binning.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}


echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.98}
input_image="${TOP_SRCDIR}/tests/data/inputs/snake_64.png"
output_sixel="${ARTIFACT_LOCAL_DIR}/kmeans-binning-soft.six"
lsqa_run_status=0

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Qk:Bsoft:N6:Msrgb:Dtrilinear:R32 \
    -p 64 \
    -o "${output_sixel}" \
    "${input_image}" || {
    echo "not ok" 1 - "img2sixel kmeans soft binning failed"
    exit 0
}

lsqa_err=$(
    set +xv
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${input_image}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

test "${lsqa_run_status}" -eq 0 || {
    echo "not ok" 1 - "kmeans soft binning lsqa failed (${lsqa_run_status})"
    exit 0
}

echo "ok" 1 - "kmeans soft binning lsqa passed"
exit 0
