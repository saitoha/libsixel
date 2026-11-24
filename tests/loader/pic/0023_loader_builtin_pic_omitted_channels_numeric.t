#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify PIC omitted-channel default values with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0088_loader_builtin_pic_omitted_channels_numeric" || {
    printf '\n%s\n' "not ok 1 - PIC omitted-channel default values"
    exit 0
}

printf '\n%s\n' "ok 1 - PIC omitted-channel default values"
exit 0
