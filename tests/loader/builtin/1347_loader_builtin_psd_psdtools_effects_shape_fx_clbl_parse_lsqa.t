#!/bin/sh
# Verify clbl parse contract remains visible on effects/shape-fx while
# keeping psd-tools LSQA parity.
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
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_shape_fx.psd"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_effects_shape_fx_expected_psdtools.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o "${output_sixel}" "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/shape-fx decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: parsed clbl=1*}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx did not parse clbl=1"
    exit 0
}

test "${trace_output#*builtin PSD: parsed clbl=0*}" != "${trace_output}" || {
    echo "not ok" 1 - "effects/shape-fx did not parse clbl=0"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "effects/shape-fx decode fell below MS-SSIM ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"
echo "ok" 1 - "effects/shape-fx keeps clbl parse and LSQA ${lsqa_floor}"
exit 0
