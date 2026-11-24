#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF disposal method 2 composition with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0078_loader_builtin_gif_disposal2_numeric" || {
    printf '\n%s\n' "not ok 1 - GIF disposal method 2 composition"
    exit 0
}

printf '\n%s\n' "ok 1 - GIF disposal method 2 composition"
exit 0
