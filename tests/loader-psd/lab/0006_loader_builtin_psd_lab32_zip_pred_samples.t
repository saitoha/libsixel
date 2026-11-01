#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded Lab samples against independent D65 values.

set -eux

printf '1..1\n'
set -v

# The fixture uses normalized D65 Lab storage; a CoreGraphics RGB
# rendering is not a sample oracle for this representation. The PPM16
# oracle allows half a 16-bit level (0.002 byte levels) plus float roundoff.
# Regenerate: python3 tests/data/loader/builtin_expected/generate_lab_sample_references.py
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psd_snake16_lab32_d65_samples.ppm" \
    "${TOP_SRCDIR}/tests/data/inputs/formats/snake16_lab32_zip_pred.psd" 'builtin:Eauto!' 0.004 || {
    echo "not ok 1 - decoded samples differ from the reference"
    exit 0
}

echo "ok 1 - decoded Lab samples match the independent D65 conversion"
exit 0
