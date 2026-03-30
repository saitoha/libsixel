#!/bin/sh
# Verify multi-layer fallback keeps decoding and emits info trace for this
# payload class after degrade policy update.

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_SRCDIR}/tests/data/inputs/formats/snake16_mode7_cmyk16_missing_composite_multilayer_knockout.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_cmyk16_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/0721_loader_builtin_psd_mode7_cmyk16_missing_composite_multilayer_knockout_unsupported_trace_output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}
trace_output=''
command_status=0

trace_output=$(set +xv; ${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" \
    --env SIXEL_TRACE_TOPIC=psd_decode \
    -Lbuiltin! -o "${output_sixel}" "${input_psd}" 2>&1) || command_status=$?

test "${command_status}" -eq 0 || {
    echo "not ok" 1 - "builtin loader failed to decode fallback payload case: ${trace_output}"
    exit 0
}

case "${trace_output}" in
    *"builtin PSD: ignoring knockout in layer fallback"*)
        ;;
    *)
        echo "not ok" 1 - "expected info trace is missing"
        exit 0
        ;;
esac

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "fallback payload decode fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "fallback payload case decodes and keeps MS-SSIM ${lsqa_floor} with expected info trace"
exit 0
