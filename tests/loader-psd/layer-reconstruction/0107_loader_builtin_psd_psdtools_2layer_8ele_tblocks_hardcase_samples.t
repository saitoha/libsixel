#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify decoded samples against the independent reference.
# Fixture/expected regeneration command:
#   python3 tests/data/psd-tools/generate_psdtools_hybrid_assets.py --download

set -eux

printf '1..1\n'
set -v

# Compare decoded values before palette quantization. The budget covers
# reference rounding and decoder storage rounding, not spatial pooling.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_2layer_8ele_tblocks_expected_psdtools.ppm" \
    "${TOP_SRCDIR}/tests/data/psd-tools/psdtools_2layer_8ele_tblocks.psd" 'builtin:Eauto!' 1.01 || {
    echo "not ok 1 - decoded samples differ from the reference"
    exit 0
}

echo "ok 1 - decoded samples match the independent reference"
exit 0
