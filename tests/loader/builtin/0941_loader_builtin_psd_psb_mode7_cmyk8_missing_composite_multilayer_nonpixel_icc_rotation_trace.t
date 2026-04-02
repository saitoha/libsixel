#!/bin/sh
# Verify PSB mode7 CMYK8 non-pixel bad/malformed ICC rotation contract.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
status=0
trace_log=''

for suffix in normal nonpixel_nopixel_tysh_descriptor_gray nonpixel_nopixel_tysh_enginedata_fillcolor_rgb nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab fill_soco_descriptor
 do
    input_psd="${base_dir}/snake16_psb_mode7_cmyk8_missing_composite_multilayer_${suffix}_bad_icc_profile.psd"
    command_status=0
    trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSB mode7 CMYK8 non-pixel bad ICC decode failed for ${suffix}: ${trace_log}"
        status=1
        break
    }

    case "${trace_log}" in
        *"embedded ICC conversion failed"*)
            ;;
        *)
            echo "not ok" 1 - "PSB mode7 CMYK8 non-pixel bad ICC failure trace is missing for ${suffix}"
            status=1
            break
            ;;
    esac
 done

test "${status}" -eq 0 || exit 0

for suffix in normal nonpixel_nopixel_tysh_descriptor_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_rgb nonpixel_nopixel_tysh_enginedata_fillcolor_values_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb nonpixel_nopixel_tysh_enginedata_fillcolor_values_gray nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_cmyk nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_named_hsb nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_gray nonpixel_nopixel_tysh_enginedata_fillcolor_stylesheet_values_lab fill_soco_descriptor_cmyk
 do
    input_psd="${base_dir}/snake16_psb_mode7_cmyk8_missing_composite_multilayer_${suffix}_malformed_resource.psd"
    command_status=0
    trace_log=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -v -Lbuiltin:cms=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSB mode7 CMYK8 non-pixel malformed ICC decode failed for ${suffix}: ${trace_log}"
        status=1
        break
    }

    case "${trace_log}" in
        *"malformed ICC resource section; skipping ICC conversion"*)
            ;;
        *)
            echo "not ok" 1 - "PSB mode7 CMYK8 non-pixel malformed ICC trace is missing for ${suffix}"
            status=1
            break
            ;;
    esac
 done

test "${status}" -eq 0 || exit 0

echo "ok" 1 - "PSB mode7 CMYK8 non-pixel bad/malformed ICC rotation contract is preserved"
exit 0
