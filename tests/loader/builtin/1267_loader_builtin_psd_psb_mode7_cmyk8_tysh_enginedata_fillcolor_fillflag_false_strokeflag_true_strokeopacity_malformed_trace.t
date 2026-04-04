#!/bin/sh
# Verify PSB mode7(4ch->CMYK8) TySh malformed StrokeOpacity is ignored with deterministic trace.
# Fixture generation commands:
#   python3 tests/data/inputs/formats/generate_psd_snake16_fixtures.py
#   python3 tests/data/inputs/formats/generate_psb_missing_composite_fixtures.py
# Reference generation command:
#   static expected asset:
#   tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_expected.ppm

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

base_dir="${TOP_SRCDIR}/tests/data/inputs/formats"
expected_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_fill_soco_gray_expected.ppm"
input_psd="${base_dir}/snake16_psb_mode7_cmyk8_missing_composite_multilayer_nonpixel_nopixel_tysh_enginedata_fillcolor_fillflag_false_strokeflag_true_strokeopacity_malformed.psd"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
lsqa_msg=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode PSB mode7 CMYK8 TySh malformed StrokeOpacity fixture: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: malformed TySh StrokeOpacity; ignoring"*) ;;
    *)
        echo "not ok" 1 - "malformed StrokeOpacity trace is missing"
        exit 0
        ;;
esac

case "${trace_output}" in
    *"builtin PSD: rendering non-pixel stroke payload in layer fallback"*) ;;
    *)
        echo "not ok" 1 - "stroke payload render trace is missing"
        exit 0
        ;;
esac

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear \
    -b "MS-SSIM:${lsqa_floor}" \
    "${expected_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "PSB mode7 CMYK8 TySh malformed StrokeOpacity decode fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSB mode7 CMYK8 TySh malformed StrokeOpacity keeps deterministic trace and MS-SSIM ${lsqa_floor}"
exit 0
