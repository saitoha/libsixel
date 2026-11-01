#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded samples against the independent reference.

set -eux

printf '1..1\n'
set -v

# Compare decoded values before palette quantization. The budget covers
# reference rounding and decoder storage rounding, not spatial pooling.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/cmyk_mft2_truncated_psd_builtin_fallback.six" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/stbi_minimal_mode7_cmyk8_mft2_truncated_icc_profile.psd" 'builtin:Eauto!' 1.01 || {
    echo "not ok 1 - decoded samples differ from the reference"
    exit 0
}

echo "ok 1 - decoded samples match the independent reference"
exit 0
