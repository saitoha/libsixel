#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF87a signature and pixels with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0071_loader_builtin_gif87a_numeric" || {
    printf '\n%s\n' "not ok 1 - GIF87a signature and pixels"
    exit 0
}

printf '\n%s\n' "ok 1 - GIF87a signature and pixels"
exit 0
