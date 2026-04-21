#!/bin/sh
# Verify builtin PSD decode reaches hard-case psd-tools baseline for advanced-blending.psd.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"
: "${LSQA_PATH:=${TOP_BUILDDIR}/assessment/lsqa}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_advanced_blending.psd"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_advanced_blending_expected_psdtools.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
command_status=0

set +e
set +xv
${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    -Lbuiltin:e=auto! -o "${output_sixel}" "${input_psd}" >/dev/null 2>&1
command_status=$?
set -e

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed on hard-case advanced-blending fixture"
    exit 0
}

${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" >/dev/null 2>&1 || {
    echo "not ok" 1 - "hard-case advanced-blending decode fell below MS-SSIM ${lsqa_floor}"
    exit 0
}

echo "ok" 1 - "hard-case advanced-blending decode reaches MS-SSIM ${lsqa_floor}"
exit 0
