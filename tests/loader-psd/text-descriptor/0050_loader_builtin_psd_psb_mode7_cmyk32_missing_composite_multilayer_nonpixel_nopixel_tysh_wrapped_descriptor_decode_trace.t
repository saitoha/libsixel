#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify PSB mode7 CMYK32 TySh wrapped descriptor payload in a non-pixel/no-pixel
# layer is interpreted as synthetic fill during missing-composite fallback.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py
# Reference generation commands:
#   magick -size 16x16 xc:'rgb(255,48,64)' -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_wrapped_descriptor.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode PSB mode7 CMYK32 TySh wrapped descriptor non-pixel/no-pixel PSD: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: ignoring non-pixel payload in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "PSB mode7 CMYK32 TySh wrapped descriptor non-pixel/no-pixel ignore info trace is missing"
        exit 0
        ;;
esac

case "${trace_output}" in
    *"builtin PSD: rendering non-pixel fill payload in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "PSB mode7 CMYK32 TySh wrapped descriptor non-pixel/no-pixel fill render trace is missing"
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

echo "ok" 1 - "PSB mode7 CMYK32 TySh wrapped descriptor non-pixel/no-pixel layer renders synthetic fill with deterministic trace and matches reference pixel values"
exit 0
