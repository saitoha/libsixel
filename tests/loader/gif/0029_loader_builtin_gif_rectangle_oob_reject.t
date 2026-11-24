#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF out-of-bounds image rectangle rejection with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0074_loader_builtin_gif_rectangle_oob_reject" || {
    printf '\n%s\n' "not ok 1 - GIF out-of-bounds image rectangle rejection"
    exit 0
}

printf '\n%s\n' "ok 1 - GIF out-of-bounds image rectangle rejection"
exit 0
