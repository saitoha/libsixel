#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
# Verify GIF illegal LZW dictionary code rejection with a minimal in-memory stream.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0077_loader_builtin_gif_illegal_lzw_code_reject" || {
    printf '\n%s\n' "not ok 1 - GIF illegal LZW dictionary code rejection"
    exit 0
}

printf '\n%s\n' "ok 1 - GIF illegal LZW dictionary code rejection"
exit 0
