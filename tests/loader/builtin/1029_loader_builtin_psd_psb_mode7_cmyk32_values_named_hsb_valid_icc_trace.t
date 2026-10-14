#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify PSB TySh EngineData named HSB + valid ICC matrix matches reference RGB values
# and avoids false failure traces.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py
#   magick -size 16x16 xc:'rgb(255,48,64)' -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test "${HAVE_LCMS2-}" = 1 || {
    printf "1..0 # SKIP lcms2 support is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm"
status=0
trace_output=''

case_name=snake16_psb_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb_valid_icc_profile
    input_psd="${base_dir}/${case_name}.psd"
    command_status=0
    trace_output=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! \
        -o /dev/null "${input_psd}" 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "builtin loader failed to decode PSB named HSB valid ICC fixture ${case_name}: ${trace_output}"
        status=1
        exit 0
    }

    case "${trace_output}" in
        *"embedded ICC conversion failed"*|*"malformed ICC resource section"*)
            echo "not ok" 1 - "unexpected ICC failure trace for ${case_name}: ${trace_output}"
            status=1
            exit 0
            ;;
        *)
            ;;
    esac

# Observe decoded samples before palette quantization. The independent
# PPM and decoded bytes can each round by half a level; reserve 0.01 for
# floating-point arithmetic in addition to that one-level sample budget.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from the independent PPM"
    exit 0
}
test "${status}" -eq 0 || exit 0

echo "ok" 1 - "PSB named HSB valid ICC matrix keeps deterministic trace matches reference pixel values"
exit 0
