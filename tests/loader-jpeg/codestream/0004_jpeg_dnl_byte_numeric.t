#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Resolve delayed height without leaving the RGB888 path.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0203_jpeg_dnl_byte_numeric" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Resolve delayed height without leaving the RGB888 path"
    exit 0
}
printf '\n%s\n' "ok 1 - Resolve delayed height without leaving the RGB888 path"
exit 0
