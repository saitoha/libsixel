#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify PSD TySh EngineData StyleRun run-length weighted 2-run decode.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
# Reference generation command:
#   python3 tests/data/loader/builtin_expected/generate_small_psd_references.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_weighted_2run_numeric.ppm"
input_psd="${base_dir}/snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_runlength_weighted_2run.psd"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o /dev/null "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode TySh StyleRun weighted 2-run fixture: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: ignoring non-pixel payload in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "ignore non-pixel payload trace is missing"
        exit 0
        ;;
esac

case "${trace_output}" in
    *"builtin PSD: rendering non-pixel fill payload in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "render non-pixel fill payload trace is missing"
        exit 0
        ;;
esac

# The reference uses the declared run weights or Normal fallback samples.
# Two byte rounding boundaries plus float roundoff permit 1.01 RGB levels.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${expected_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from independent values"
    exit 0
}

echo "ok" 1 - "PSD TySh StyleRun weighted 2-run keeps deterministic trace and independent pixel values"
exit 0
