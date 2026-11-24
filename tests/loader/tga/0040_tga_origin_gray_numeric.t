#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Read grayscale palette entries directly after the image ID.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0240_tga_origin_gray_numeric" 1>&2 || {
    printf '\n%s\n' "not ok 1 - Read grayscale palette entries directly after the image ID"
    exit 0
}
printf '\n%s\n' "ok 1 - Read grayscale palette entries directly after the image ID"
exit 0
