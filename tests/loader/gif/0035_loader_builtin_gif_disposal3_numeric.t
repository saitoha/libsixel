#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF disposal method 3 restores exact pre-frame pixels.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0103_loader_builtin_gif_disposal3_numeric" || {
    printf '\n%s\n' "not ok 1 - GIF disposal method 3 exact restoration"
    exit 0
}

printf '\n%s\n' "ok 1 - GIF disposal method 3 exact restoration"
exit 0
