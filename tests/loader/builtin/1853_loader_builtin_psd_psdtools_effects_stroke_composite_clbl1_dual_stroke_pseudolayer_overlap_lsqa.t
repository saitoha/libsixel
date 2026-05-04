#!/bin/sh
# Verify deferred dual stroke overlap pseudo-layer keeps quality contract.
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
trace_tail=''
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

trace_tail="${trace_output#*builtin PSD: applying deferred dual-stroke overlap decomposition on clipped group*}"
test "${trace_tail}" != "${trace_output}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred overlap decomposition trace"
    exit 0
}

test "${trace_tail#*builtin PSD: applying deferred dual-stroke pseudo-layer blend on clipped group*}" \
    != "${trace_tail}" || {
    echo "not ok" 1 - \
        "effects/stroke-composite missing deferred pseudo-layer overlap trace"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - \
        "effects/stroke-composite deferred pseudo-layer overlap LSQA fell below ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"

echo "ok" 1 - "effects/stroke-composite keeps deferred overlap pseudo-layer quality"
exit 0
