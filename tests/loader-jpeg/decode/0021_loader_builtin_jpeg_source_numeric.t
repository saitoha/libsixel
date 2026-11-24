#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/jpeg.md
# Verify unmanaged JPEG samples against independently decoded JFIF values.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0183_loader_builtin_jpeg_source_numeric" || {
    printf '\n%s\n' "not ok 1 - independent JPEG source samples"
    exit 0
}
printf '\n%s\n' "ok 1 - independent JPEG source samples"
exit 0
