#!/bin/sh
# Verify PSD CMYK8 TySh /DefaultStyleSheet /Color /Values [/DeviceRGB ...] decode.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
# Reference generation command:
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
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_expected.ppm"
input_psd="${base_dir}/snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_default_stylesheet_color_values_named_device_rgb.psd"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode CMYK8 TySh /DefaultStyleSheet /Color DeviceRGB fixture: ${trace_output}"
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

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "CMYK8 TySh /DefaultStyleSheet /Color DeviceRGB decode fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSD CMYK8 TySh /DefaultStyleSheet /Color DeviceRGB keeps deterministic trace and MS-SSIM ${lsqa_floor}"
exit 0
