#!/bin/sh
# Verify automatic Kmeans binning retains the explicit hard-policy output.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
reference_image="${TOP_SRCDIR}/tests/data/inputs/snake_16.png"
artifact_dir="${ARTIFACT_LOCAL_DIR}"
auto_output="${artifact_dir}/0034-quantize-kmeans-auto-binning-$$.six"
hard_output="${artifact_dir}/0034-quantize-kmeans-hard-binning-$$.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -p 16 -Qkmeans \
    "${input_image}" >"${auto_output}" || {
    echo "not ok" 1 - "automatic Kmeans binning conversion failed"
    exit 0
}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" --binning-policy=hard \
    -p 16 -Qkmeans "${input_image}" >"${hard_output}" || {
    echo "not ok" 1 - "explicit hard Kmeans binning conversion failed"
    exit 0
}

cmp -s "${auto_output}" "${hard_output}" || {
    echo "not ok" 1 - "automatic and hard Kmeans binning output differ"
    exit 0
}

lsqa_error=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" \
    -b "MS-SSIM:0.98" "${reference_image}" \
    "${auto_output}" 2>&1) || lsqa_status=$?
test "${lsqa_status:-0}" -eq 0 || {
    echo "not ok" 1 - "automatic Kmeans binning image quality regressed"
    printf "# %s\n" "${lsqa_error}"
    exit 0
}

echo "ok" 1 - "automatic Kmeans binning retains hard-policy output"
exit 0
