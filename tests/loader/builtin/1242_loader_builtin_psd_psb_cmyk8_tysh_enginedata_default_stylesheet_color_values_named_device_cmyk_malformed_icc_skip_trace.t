#!/bin/sh
# Verify PSB CMYK8 TySh /DefaultStyleSheet /Color DeviceCMYK keeps malformed ICC skip trace.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py

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

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_psb_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_cmyk_malformed_resource.psd"
trace_log=''
command_status=0

trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
    "${input_psd}" -o /dev/null 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "malformed ICC decode failed: ${trace_log}"
    exit 0
}

case "${trace_log}" in
    *"malformed ICC resource section; skipping ICC conversion"*)
        ;;
    *)
        echo "not ok" 1 - "malformed ICC skip trace is missing"
        exit 0
        ;;
esac

echo "ok" 1 - "PSB CMYK8 TySh /DefaultStyleSheet /Color DeviceCMYK malformed ICC preserves skip trace"
exit 0
