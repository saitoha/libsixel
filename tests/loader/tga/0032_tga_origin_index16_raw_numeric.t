#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Use the full sixteen-bit colormap origin for raw source indices.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0232_tga_origin_index16_raw_numeric" 1>&2 || {
    echo "not ok 1 - Use the full sixteen-bit colormap origin for raw source indices"
    exit 0
}
echo "ok 1 - Use the full sixteen-bit colormap origin for raw source indices"
exit 0
