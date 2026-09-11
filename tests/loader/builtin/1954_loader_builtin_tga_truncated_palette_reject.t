#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify TGA truncated palette rejection with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0086_loader_builtin_tga_truncated_palette_reject" || {
    echo "not ok 1 - TGA truncated palette rejection"
    exit 0
}

echo "ok 1 - TGA truncated palette rejection"
exit 0
