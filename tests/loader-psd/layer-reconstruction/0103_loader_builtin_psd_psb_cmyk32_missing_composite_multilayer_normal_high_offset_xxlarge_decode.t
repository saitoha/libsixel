#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin loader reconstructs PSB mode4 CMYK32 multi-layer image at xxlarge high-offset layout.
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

input_psd="${TOP_BUILDDIR}/tests/data/inputs/formats/snake16_psb_cmyk32_missing_composite_multilayer_normal_high_offset_xxlarge.psd"
reference_ppm="${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_multilayer_cmyk32_normal_expected.ppm"

${SIXEL_RUNTIME-} "${IMG2SIXEL_PATH}" -L builtin! "${input_psd}" >/dev/null || {
    echo "not ok" 1 - "builtin loader failed to decode PSB mode4 CMYK32 multi-layer(normal high-offset-xxlarge)"
    exit 0
}

# Observe decoded samples before palette quantization. The independent
# PPM and decoded bytes can each round by half a level; reserve 0.01 for
# floating-point arithmetic in addition to that one-level sample budget.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_values \
    "${reference_ppm}" "${input_psd}" 'builtin:cms_engine=auto!' 1.01 || {
    echo "not ok 1 - decoded PSD samples differ from the independent PPM"
    exit 0
}

echo "ok" 1 - "PSB mode4 CMYK32 multi-layer(normal high-offset-xxlarge) matches reference pixel values against expected PPM"
exit 0
