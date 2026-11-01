#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify multi-layer fallback keeps decoding and emits info trace for this
# payload class after degrade policy update.
# Reference generation command:
#   python3 tests/data/loader/builtin_expected/generate_small_psd_references.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_rgb8_missing_composite_multilayer_unknown_blend.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_unknown_blend_numeric.ppm"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode fallback payload case: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: unknown layer blend mode; falling back to Normal"*)
        ;;
    *)
        echo "not ok" 1 - "expected info trace is missing"
        exit 0
        ;;
esac

# The reference uses the declared run weights or Normal fallback samples.
# Two byte rounding boundaries plus float roundoff permit 1.01 RGB levels.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from independent values"
    exit 0
}

echo "ok" 1 - "fallback payload case decodes and keeps independent pixel values with expected info trace"
exit 0
