#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/psd.md
# Policy: docs/misc/platforms/openvms.md
# Compare all Duotone display samples with an independent Lab-table oracle.
# Regeneration: python3 tests/data/psd-tools/generate_duotone_reference.py \
#   tests/data/psd-tools/psdtools_colormodes_4x4_8bit_duotone.psd \
#   tests/data/loader/builtin_expected/psdtools_colormodes_4x4_8bit_duotone_expected_srgb.ppm --srgb
# Coverage: OV-13
set -eux

echo "1..1"
set -v

# Regular files avoid GNV short-pipe EOF behavior. Half a byte level plus
# float/reference roundoff bounds the independent 8-bit display reference.
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/loader/builtin_expected/psdtools_colormodes_4x4_8bit_duotone_expected_srgb.ppm" \
    "${TOP_SRCDIR}/tests/data/psd-tools/psdtools_colormodes_4x4_8bit_duotone.psd" \
    'builtin:cms_engine=none!' 0.51 || {
    echo "not ok 1 - Duotone display samples differ from the independent oracle"
    exit 0
}
echo "ok 1 - Duotone display samples match the independent oracle"
exit 0
