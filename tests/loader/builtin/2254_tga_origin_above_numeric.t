#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Map an index above the declared palette range to its first color.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0238_tga_origin_above_numeric" 1>&2 || {
    echo "not ok 1 - Map an index above the declared palette range to its first color"
    exit 0
}
echo "ok 1 - Map an index above the declared palette range to its first color"
exit 0
