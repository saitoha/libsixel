#!/bin/sh
# Verify clbl=1 pre-subset overlay suppression keeps stroke-composite quality.
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
set +x
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_effects_stroke_composite_expected_psdtools.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.95}
trace_output=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o "${output_sixel}" "${input_psd}" 2>&1) || \
    command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

test "${trace_output#*builtin PSD: suppressing clbl=1 deferred base solid/gradient overlays*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing clbl=1 overlay suppression trace"
    exit 0
}

test "${trace_output#*builtin PSD: applying clip-weighted deferred gradient overlay in layer fallback*}" \
    != "${trace_output}" || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred gradient overlay trace"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - \
        "effects/stroke-composite pre-subset overlay suppression LSQA fell below ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"

echo "ok" 1 - "effects/stroke-composite keeps pre-subset overlay suppression quality"
exit 0
