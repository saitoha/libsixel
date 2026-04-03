#!/bin/sh
# Verify short-component PSD TySh EngineData /Color /Values [/DeviceRGB ...]
# payload keeps decode success and follows current fallback trace contract.
# Reference generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   magick tests/data/inputs/formats/snake16_cmyk8_raw.psd \
#       -colorspace sRGB -depth 8 -define ppm:format=raw \
#       PPM:tests/data/loader/builtin_expected/psd_snake16_cmyk8_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_color_values_named_device_rgb_short_payload.psd"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode short-component PSD TySh /Color /DeviceRGB payload fixture: ${trace_output}"
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
        echo "not ok" 1 - "render non-pixel fill trace is missing"
        exit 0
        ;;
esac

case "${trace_output}" in
    *"builtin PSD: malformed non-pixel fill payload; skipping layer"*)
        echo "not ok" 1 - "short payload unexpectedly treated as malformed skip"
        exit 0
        ;;
    *)
        ;;
esac

echo "ok" 1 - "short-component PSD TySh /Color /DeviceRGB payload follows fallback trace contract"
exit 0
