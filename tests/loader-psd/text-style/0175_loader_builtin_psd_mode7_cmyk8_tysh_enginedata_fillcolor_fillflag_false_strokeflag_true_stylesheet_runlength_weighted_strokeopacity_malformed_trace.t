#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify malformed weighted StrokeOpacity run is ignored per-run and decode continues.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
# Reference generation command:
#   magick tests/data/loader/builtin_expected/psd_snake16_cmyk8_expected.ppm \
#       tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_expected.ppm \
#       -set colorspace sRGB -colorspace RGB \
#       -define compose:args=50 -compose blend -composite -colorspace sRGB \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_opacity_050_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_opacity_050_expected.ppm"
input_psd="${base_dir}/snake16_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_wso_malformed.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode malformed weighted StrokeOpacity fixture: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: rendering non-pixel stroke payload in layer fallback"*) ;;
    *)
        echo "not ok" 1 - "stroke payload render trace is missing"
        exit 0
        ;;
esac

case "${trace_output}" in
    *"builtin PSD: malformed TySh StrokeOpacity; ignoring"*) ;;
    *)
        echo "not ok" 1 - "malformed StrokeOpacity trace is missing for malformed weighted run"
        exit 0
        ;;
esac

# Observe decoded samples before palette quantization. The independent
# PPM and decoded bytes can each round by half a level; reserve 0.01 for
# floating-point arithmetic in addition to that one-level sample budget.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${expected_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from the independent PPM"
    exit 0
}

echo "ok" 1 - "malformed weighted StrokeOpacity keeps deterministic trace matches reference pixel values"
exit 0
