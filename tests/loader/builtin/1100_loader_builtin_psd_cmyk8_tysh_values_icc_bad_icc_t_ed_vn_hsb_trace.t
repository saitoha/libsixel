#!/bin/sh
# Verify PSD native CMYK8 TySh EngineData /FillColor /Values ICC trace contract.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   python3 tests/data/inputs/formats/generate_psd_policy_trace_fixtures.py

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

suffix=nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_hsb
    input_psd="${base_dir}/snake16_cmyk8_missing_composite_multilayer_${suffix}_valid_icc_profile.psd"
    command_status=0
    trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSD CMYK8 TySh /Values valid ICC decode failed for ${suffix}: ${trace_log}"
        status=1
        exit 0
    }

    case "${trace_log}" in
        *"embedded ICC conversion failed"*)
            echo "not ok" 1 - "PSD CMYK8 TySh /Values valid ICC emitted false failure trace for ${suffix}"
            status=1
            exit 0
            ;;
    esac

    input_psd="${base_dir}/snake16_cmyk8_missing_composite_multilayer_${suffix}_bad_icc_profile.psd"
    command_status=0
    trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSD CMYK8 TySh /Values bad ICC decode failed for ${suffix}: ${trace_log}"
        status=1
        exit 0
    }

    case "${trace_log}" in
        *"embedded ICC conversion failed"*)
            ;;
        *)
            echo "not ok" 1 - "PSD CMYK8 TySh /Values bad ICC failure trace is missing for ${suffix}"
            status=1
            exit 0
            ;;
    esac

    input_psd="${base_dir}/snake16_cmyk8_missing_composite_multilayer_${suffix}_malformed_resource.psd"
    command_status=0
    trace_log=$(set +xv; SIXEL_TRACE_TOPIC=loader ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -Lbuiltin:cms=auto! \
        "${input_psd}" -o /dev/null 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "PSD CMYK8 TySh /Values malformed ICC decode failed for ${suffix}: ${trace_log}"
        status=1
        exit 0
    }

    case "${trace_log}" in
        *"malformed ICC resource section; skipping ICC conversion"*)
            ;;
        *)
            echo "not ok" 1 - "PSD CMYK8 TySh /Values malformed ICC trace is missing for ${suffix}"
            status=1
            exit 0
            ;;
    esac
test "${status}" -eq 0 || exit 0

echo "ok" 1 - "PSD CMYK8 TySh /Values ICC trace contract is preserved"
exit 0
