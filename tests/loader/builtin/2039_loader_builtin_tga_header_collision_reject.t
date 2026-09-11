#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Verify a conflicting TGA palette flag cannot claim a direct image.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0173_loader_builtin_tga_header_collision_reject" 1>&2 || {
    echo "not ok" 1 - "TGA conflicting header rejection"
    exit 0
}
echo "ok" 1 - "TGA conflicting header rejection"
exit 0
