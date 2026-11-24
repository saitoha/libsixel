#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/png-source-precision.md
# Preserve every source component after a qualified icc byte prefix failure.
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    loader/0193_png_metadata_failures 10 \
    "${TOP_SRCDIR}/tests/data/color-numeric/png-chrm-8-rgb-zero-icc-chrm.png" || {
    printf '\n%s\n' "not ok 1 - PNG icc byte prefix failure"
    exit 0
}
printf '\n%s\n' "ok 1 - PNG icc byte prefix failure"
exit 0
