#!/bin/sh
# Verify builtin PSD decode matches coregraphics reference for gradient-fill.psd.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_gradient_fill.psd"
expected_six="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_gradient_fill_expected_coregraphics.six"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed on psd-tools gradient-fill fixture: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"unsupported"*)
        echo "not ok" 1 - "unexpected unsupported trace for psd-tools gradient-fill fixture"
        exit 0
        ;;
esac

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_six}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "psd-tools gradient-fill decode fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSD psd-tools gradient-fill decode keeps MS-SSIM ${lsqa_floor}"
exit 0
