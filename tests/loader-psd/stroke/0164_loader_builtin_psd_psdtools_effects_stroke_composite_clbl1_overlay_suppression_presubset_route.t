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
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
trace_output=''
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

case "${trace_output}" in *builtin\ PSD:\ suppressing\ clbl=1\ deferred\ base\ solid/gradient\ overlays**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing clbl=1 overlay suppression trace"
    exit 0
}

case "${trace_output}" in *builtin\ PSD:\ applying\ clip-weighted\ deferred\ gradient\ overlay\ in\ layer\ fallback**) true ;; *) false ;; esac || {
    echo "not ok" 1 - "effects/stroke-composite missing deferred gradient overlay trace"
    exit 0
}

# This case observes the named compositor route and output dimensions.
# Independent image quality remains owned by the existing hardcase test
# 1318_loader_builtin_psd_psdtools_effects_stroke_composite_hardcase_lsqa.t.
# That known rendering limitation must not be hidden by a lower threshold.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_dimensions \
    "${output_sixel}" 256 256 || {
    echo "not ok 1 - compositor output dimensions differ"
    exit 0
}

echo "ok 1 - compositor route and output dimensions match"
exit 0
