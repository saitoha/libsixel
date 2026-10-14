#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify the named compositor route and exact output dimensions.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

: "${IMG2SIXEL_PATH:=${TOP_BUILDDIR}/converters/img2sixel}"

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
set +x
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/psd-tools/psdtools_effects_stroke_composite.psd"
auto_output="${ARTIFACT_LOCAL_DIR}/output-auto.six"
none_output="${ARTIFACT_LOCAL_DIR}/output-none.six"
auto_trace=''
none_trace=''
auto_status=0
none_status=0

auto_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=auto \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o "${auto_output}" "${input_psd}" 2>&1) || \
    auto_status=$?

test "${auto_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite auto decode failed"
    exit 0
}

none_trace=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --lookup-policy=none \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin:Eauto! -o "${none_output}" "${input_psd}" 2>&1) || \
    none_status=$?

test "${none_status}" -eq 0 || {
    echo "not ok" 1 - "effects/stroke-composite none decode failed"
    exit 0
}

case "${auto_trace}" in *builtin\ PSD:\ applying\ effect-priority\ dual-stroke\ overlap\ on\ clipped\ group**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite auto decode missed FXPRI overlap trace"
    exit 0
}

case "${none_trace}" in *builtin\ PSD:\ applying\ effect-priority\ dual-stroke\ overlap\ on\ clipped\ group**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite none decode missed FXPRI overlap trace"
    exit 0
}

# This case observes the named compositor route and output dimensions.
# Independent image quality remains owned by the existing hardcase test
# 1318_loader_builtin_psd_psdtools_effects_stroke_composite_hardcase_lsqa.t.
# That known rendering limitation must not be hidden by a lower threshold.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_dimensions \
    "${auto_output}" 256 256 || {
    echo "not ok 1 - compositor output dimensions differ"
    exit 0
}
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_dimensions \
    "${none_output}" 256 256 || {
    echo "not ok 1 - compositor output dimensions differ"
    exit 0
}

echo "ok 1 - compositor route and output dimensions match"
exit 0
