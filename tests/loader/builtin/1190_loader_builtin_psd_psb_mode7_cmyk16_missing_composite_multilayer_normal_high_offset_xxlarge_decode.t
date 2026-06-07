#!/bin/sh
# Verify builtin loader reconstructs PSB mode7(4ch->CMYK)16 multi-layer image at xxlarge high-offset layout.
# Fixture is prepared under TOP_BUILDDIR/tests/data/inputs/formats
# by tests/_static/sh/prepare-psb-large-fixtures.sh

set -eux

test "${HAVE_IMG2SIXEL-}" = 1 || {
    printf "1..0 # SKIP img2sixel is disabled in this build\n"
    exit 0
}

test -n "${SIXEL_TEST_GZIP-}" || {
    printf "1..0 # SKIP gzip is unavailable for compressed PSB fixture\n"
    exit 0
}

test "${SIXEL_TEST_C_COMPILER_ID-}" = msvc && {
    printf "1..0 # SKIP MSVC cmd runner is too slow for xxlarge PSB fixtures\n"
    exit 0
}

echo "1..1"
set -v
test -d "${ARTIFACT_LOCAL_DIR}" || mkdir -p "${ARTIFACT_LOCAL_DIR}"

input_psd="${TOP_BUILDDIR}/tests/data/inputs/formats/snake16_psb_mode7_cmyk16_missing_composite_multilayer_normal_high_offset_xxlarge.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_cmyk16_normal_expected.ppm"
output_sixel="${ARTIFACT_LOCAL_DIR}/output.six"
lsqa_floor=${LSQA_MS_SSIM_FLOOR:-0.995}

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >"${output_sixel}" || {
    echo "not ok" 1 - "builtin loader failed to decode PSB mode7(4ch->CMYK)16 multi-layer(normal high-offset-xxlarge)"
    exit 0
}

lsqa_msg=$(set +xv; ${SIXEL_RUNTIME-} "${LSQA_PATH}" -m MS-SSIM -W linear -b "MS-SSIM:${lsqa_floor}" \
    "${reference_ppm}" "${output_sixel}" 2>&1) || {
    echo "not ok" 1 - "PSB mode7(4ch->CMYK)16 multi-layer(normal high-offset-xxlarge) fell below MS-SSIM ${lsqa_floor}: ${lsqa_msg}"
    exit 0
}

echo "ok" 1 - "PSB mode7(4ch->CMYK)16 multi-layer(normal high-offset-xxlarge) keeps MS-SSIM ${lsqa_floor} against expected PPM"
exit 0
