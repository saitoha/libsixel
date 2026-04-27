#!/bin/sh
# Verify deferred stroke uses post-effect group backdrop without quality drop.
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
stroke_tail=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o "${output_sixel}" "${input_psd}" 2>&1) || \
    command_status=$?

: "${trace_output}"

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite decode failed"
    exit 0
}

stroke_tail="${trace_output#*builtin PSD: applying deferred stroke on clipped group*}"
test "${stroke_tail}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred stroke trace"
    exit 0
}

test "${stroke_tail#*builtin PSD: using post-effect offscreen group backdrop for deferred stroke blend*}" \
    != "${stroke_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing post-effect backdrop trace after deferred stroke"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - \
        "effects/stroke-composite deferred stroke backdrop LSQA fell below ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"

echo "ok" 1 - "effects/stroke-composite keeps deferred stroke backdrop quality"
exit 0
