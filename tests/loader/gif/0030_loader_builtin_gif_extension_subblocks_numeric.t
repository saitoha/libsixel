#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF extension sub-block skipping with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0075_loader_builtin_gif_extension_subblocks_numeric" || {
    printf '\n%s\n' "not ok 1 - GIF extension sub-block skipping"
    exit 0
}

printf '\n%s\n' "ok 1 - GIF extension sub-block skipping"
exit 0
