#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin PSD decode reaches psd-tools baseline for
# vector-mask-disabled.psd.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

# Quality is measured at the explicitly requested encoder output size.
# Nearest resampling precedes palette construction; the independent reference
# follows the same integer pixel replication. Native samples have separate
# decoder tests. No measured SIXEL output is enlarged for assessment.
# Regenerate references: python3 tests/data/loader/quality/generate_references.py

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

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_vector_mask_disabled.psd"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/quality/1316-psdtools_vector_mask_disabled_expected_psdtools.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -r nearest -w 200 -h 300 \
    -Lbuiltin:Eauto! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?
: "${trace_output}"

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "vector-mask-disabled decode failed"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "vector-mask-disabled decode fell below MS-SSIM ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"

echo "ok" 1 - "vector-mask-disabled decode keeps MS-SSIM ${lsqa_floor}"
exit 0
