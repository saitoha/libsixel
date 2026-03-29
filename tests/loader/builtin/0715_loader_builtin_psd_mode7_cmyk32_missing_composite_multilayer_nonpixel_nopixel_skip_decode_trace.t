#!/bin/sh
# Verify mode7(4ch->CMYK32) non-pixel-only layer (no decodable pixel channels)
# is skipped during multi-layer fallback with informational trace.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick tests/data/inputs/formats/snake_64.png -resize 16x16! -alpha off \
#       -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_cmyk32_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_tysh.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_cmyk32_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/psd_mode7_cmyk32_missing_composite_multilayer_nonpixel_nopixel_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! "${input_psd}" -o "${output_sixel}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode mode7 CMYK32 non-pixel/no-pixel multi-layer PSD: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: ignoring non-pixel payload in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "mode7 CMYK32 non-pixel/no-pixel ignore info trace is missing"
        exit 0
        ;;
esac

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "mode7 CMYK32 non-pixel/no-pixel multi-layer decode fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "mode7 CMYK32 non-pixel/no-pixel layer is skipped with info trace and decode keeps MS-SSIM ${lsqa_floor}"
exit 0
