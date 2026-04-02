#!/bin/sh
# Verify PSD TySh EngineData FillColor /Values([/CMYK ...]) parsing for
# mode4/mode7 CMYK(8/16/32bpc) non-pixel/no-pixel fallback layers.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick -size 16x16 xc:'rgb(255,48,64)' -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
status=0
trace_output=''
lsqa_msg=''

case_name=snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_values_named_cmyk
    input_psd="${base_dir}/${case_name}.psd"
    command_status=0
    trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
        --env SIXEL_TRACE_TOPIC=psd_decode \
        -Lbuiltin! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?

    test "${command_status}" -eq 0 || {
        echo "not ok" 1 - "builtin loader failed to decode TySh EngineData named values CMYK fixture ${case_name}: ${trace_output}"
        status=1
        exit 0
    }

    case "${trace_output}" in
        *"builtin PSD: ignoring non-pixel payload in layer fallback"*)
            ;;
        *)
            echo "not ok" 1 - "ignore non-pixel payload trace is missing for ${case_name}"
            status=1
            exit 0
            ;;
    esac

    case "${trace_output}" in
        *"builtin PSD: rendering non-pixel fill payload in layer fallback"*)
            ;;
        *)
            echo "not ok" 1 - "render non-pixel fill payload trace is missing for ${case_name}"
            status=1
            exit 0
            ;;
    esac

    lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
        -b "MS-SSIM:${lsqa_floor}" \
        "${reference_ppm}" "${output_sixel}" 2>&1) || {
        echo "not ok" 1 - "TySh EngineData named values CMYK decode fell below MS-SSIM ${lsqa_floor} for ${case_name}: ${lsqa_msg}"
        status=1
        exit 0
    }
test "${status}" -eq 0 || exit 0

echo "ok" 1 - "PSD TySh EngineData named values CMYK matrix keeps deterministic trace and MS-SSIM ${lsqa_floor}"
exit 0
