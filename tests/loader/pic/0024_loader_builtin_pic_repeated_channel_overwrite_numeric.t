#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/pic.md
# Verify PIC repeated-channel packet overwrite order with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0089_loader_builtin_pic_repeated_channel_overwrite_numeric" || {
    printf '\n%s\n' "not ok 1 - PIC repeated-channel packet overwrite order"
    exit 0
}

printf '\n%s\n' "ok 1 - PIC repeated-channel packet overwrite order"
exit 0
