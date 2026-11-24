#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/hdr.md
# Verify legacy RLE cannot repeat before a source pixel exists.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0133_loader_builtin_hdr_legacy_repeat_first_reject" || {
    printf '\n%s\n' "not ok 1 - HDR legacy RLE rejects a leading repeat"
    exit 0
}

printf '\n%s\n' "ok 1 - HDR legacy RLE rejects a leading repeat"
exit 0
