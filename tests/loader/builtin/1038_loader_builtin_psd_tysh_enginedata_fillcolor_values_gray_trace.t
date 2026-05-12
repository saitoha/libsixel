#!/bin/sh
# Verify PSB mode7 CMYK32 non-pixel valid ICC matrix avoids false embedded-ICC failure traces.
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

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
status=0
trace_log=''
suffix=nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray
    input_psd="${base_dir}/snake16_psb_mode7_cmyk32_missing_composite_multilayer_${suffix}_valid_icc_profile.psd"
    command_status=0
    trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms_engine=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSB mode7 CMYK32 non-pixel valid ICC matrix decode failed for ${suffix}: ${trace_log}"
        status=1
        exit 0
    }

    case "${trace_log}" in
        *"embedded ICC conversion failed"*)
            echo "not ok" 1 - "PSB mode7 CMYK32 non-pixel valid ICC matrix emitted false failure trace for ${suffix}"
            status=1
            exit 0
            ;;
    esac
test "${status}" -eq 0 || exit 0

echo "ok" 1 - "PSB mode7 CMYK32 non-pixel valid ICC matrix avoids false embedded-ICC failure traces"
exit 0
