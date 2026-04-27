#!/bin/sh
# Verify FXPRI inside overlap keeps quality in auto/none lookup policies.
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
auto_output="${ARTIFACT_LOCAL_DIR}/output-auto.six"
none_output="${ARTIFACT_LOCAL_DIR}/output-none.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.95}
auto_trace=''
none_trace=''
lsqa_msg=''
auto_status=0
none_status=0

auto_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=auto \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o "${auto_output}" "${input_psd}" 2>&1) || \
    auto_status=$?

test "${auto_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite auto decode failed"
    exit 0
}

none_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:e=auto! -o "${none_output}" "${input_psd}" 2>&1) || \
    none_status=$?

test "${none_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite none decode failed"
    exit 0
}

test "${auto_trace#*builtin PSD: applying effect-priority dual-stroke overlap on clipped group*}" \
    != "${auto_trace}" || {
    echo "not ok" 1 - "effects/stroke-composite auto decode missed FXPRI overlap trace"
    exit 0
}

test "${none_trace#*builtin PSD: applying effect-priority dual-stroke overlap on clipped group*}" \
    != "${none_trace}" || {
    echo "not ok" 1 - "effects/stroke-composite none decode missed FXPRI overlap trace"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${auto_output}" 2>&1) || {
    echo "not ok" 1 - "effects/stroke-composite auto LSQA fell below ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${none_output}" 2>&1) || {
    echo "not ok" 1 - "effects/stroke-composite none LSQA fell below ${lsqa_floor}"
    exit 0
}

: "${lsqa_msg}"

echo "ok" 1 - "effects/stroke-composite keeps FXPRI overlap quality in auto/none"
exit 0
