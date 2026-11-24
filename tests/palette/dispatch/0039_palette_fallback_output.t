#!/bin/sh
# Verify that table and shift-based palette expansion produce equal pixels.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "palette/fallback/0007_palette_fallback_output" || {
    printf '\nnot ok 1 - table and fallback expansion differ\n'
    exit 0
}

printf '\nok 1 - table and fallback expansion produce equal pixels\n'
exit 0
