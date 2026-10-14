#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify multi-layer fallback keeps decoding and emits info trace for this
# payload class after degrade policy update.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk8_missing_composite_multilayer_layer_effects.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_cmyk8_expected.ppm"
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
    *"builtin PSD: ignoring layer effects in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "expected info trace is missing"
        exit 0
        ;;
esac

# This contract concerns decoded samples, before palette quantization.
# The independent 8-bit PPM and decoded 8-bit samples can each round by
# half a level; allow one RGB byte level plus float conversion roundoff.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from the independent PPM"
    exit 0
}

echo "ok" 1 - "fallback payload case decodes and matches reference pixel values with expected info trace"
exit 0
