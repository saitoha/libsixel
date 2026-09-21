#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Map an index below the declared origin to the first stored color.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0237_tga_origin_below_numeric" 1>&2 || {
    echo "not ok 1 - Map an index below the declared origin to the first stored color"
    exit 0
}
echo "ok 1 - Map an index below the declared origin to the first stored color"
exit 0
