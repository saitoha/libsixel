#!/bin/sh
# Verify canonical WIC ico_minsize env name applies without -L suboptions.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n";
    exit 0
}
test "${HAVE_WIC-}" = 1 || {
    printf "1..0 # SKIP wic loader is unavailable\n";
    exit 0
}
test "${RUNTIME_ENV_IS_WINE-0}" -eq 1 && {
    printf "1..0 # SKIP WIC is unavailable under wine\n";
    exit 0
}


printf '1..1\n'
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

lsqa_floor=0.96
image_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-ico-multisize.ico"
reference_path="${TOP_SRCDIR}/tests/data/inputs/formats/snake-32.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/wic_ico_minsize_env_canonical.six"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_LOADER_WIC_ICO_MINSIZE=30 \
    -Lwic! \
    "${image_path}" >"${output_sixel}" || {
    echo "not ok" 1 - "wic ico env canonical-name conversion failed"
    exit 0
}

lsqa_err=$(
    ${SIXEL_RUNTIME-} "${LSQA_PATH}" -b "MS-SSIM:${lsqa_floor}" "${reference_path}" "${output_sixel}" 2>&1
) || lsqa_run_status=$?

test "${lsqa_run_status:-0}" -eq 0 && {
    echo "ok" 1 - "wic ico canonical env name quality meets baseline"
    exit 0
}

test "${lsqa_run_status}" -eq 5 && {
    echo "not ok" 1 - "${lsqa_err}"
    exit 0
}

echo "not ok" 1 - "wic ico canonical env name quality regressed"
exit 0
