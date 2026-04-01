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
for suffix in \
    normal \
    nonpixel_tysh_descriptor \
    nonpixel_nopixel_tysh_descriptor \
    nonpixel_nopixel_tysh_descriptor_gray \
    nonpixel_nopixel_tysh_descriptor_cmyk \
    nonpixel_nopixel_tysh_descriptor_hsb \
    nonpixel_nopixel_tysh_descriptor_lab \
    nonpixel_nopixel_tysh_wrapped_descriptor \
    nonpixel_nopixel_tysh_wrapped_unknown_descriptor \
    nonpixel_nopixel_tysh_wrapped_malformed_descriptor \
    fill_soco_descriptor \
    fill_soco_descriptor_cmyk \
    fill_soco_descriptor_gray \
    fill_soco_descriptor_hsb \
    fill_soco_descriptor_lab \
    fill_gdfl_descriptor \
    fill_gdfl_descriptor_cmyk \
    fill_gdfl_descriptor_gray \
    fill_gdfl_descriptor_hsb \
    fill_gdfl_descriptor_lab \
    fill_ptfl_descriptor \
    fill_ptfl_descriptor_cmyk \
    fill_ptfl_descriptor_gray \
    fill_ptfl_descriptor_hsb \
    fill_ptfl_descriptor_lab
 do
    input_psd="${base_dir}/snake16_psb_mode7_cmyk32_missing_composite_multilayer_${suffix}_valid_icc_profile.psd"
    command_status=0
    trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSB mode7 CMYK32 non-pixel valid ICC matrix decode failed for ${suffix}: ${trace_log}"
        status=1
        break
    }

    case "${trace_log}" in
        *"embedded ICC conversion failed"*)
            echo "not ok" 1 - "PSB mode7 CMYK32 non-pixel valid ICC matrix emitted false failure trace for ${suffix}"
            status=1
            break
            ;;
    esac
 done

test "${status}" -eq 0 || exit 0

echo "ok" 1 - "PSB mode7 CMYK32 non-pixel valid ICC matrix avoids false embedded-ICC failure traces"
exit 0
