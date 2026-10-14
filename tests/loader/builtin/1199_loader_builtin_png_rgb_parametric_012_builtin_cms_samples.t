#!/bin/sh
# Policy: docs/loader/builtin-cms.md
# Policy: docs/loader/color-management/README.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Verify builtin PNG loader reaches parametric ICC path with builtin CMS.

set -eux

printf '1..1\n'
set -v

# Independent ICC parametric-curve equations and PCS adaptation, with a
# half-PPM16-level budget plus float roundoff, before palette quantization.
# Regenerate: python3 tools/generate_cms_quality_references.py
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" loader/image_display_values \
    "${TOP_SRCDIR}/tests/data/colormgmt/reference/custom/rgb_parametric_012_analytic.ppm" \
    "${TOP_SRCDIR}/tests/data/colormgmt/input/custom/rgb_parametric_012.png" \
    'builtin:cms_engine=builtin!' 0.004 || {
    echo "not ok 1 - parametric ICC samples differ from the analytic oracle"
    exit 0
}

echo "ok 1 - parametric ICC samples match the analytic oracle"
exit 0
