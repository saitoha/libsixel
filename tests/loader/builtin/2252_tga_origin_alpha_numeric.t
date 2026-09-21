#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/tga.md
# Preserve the zero-based transparent palette key after index rebasing.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0236_tga_origin_alpha_numeric" 1>&2 || {
    echo "not ok 1 - Preserve the zero-based transparent palette key after index rebasing"
    exit 0
}
echo "ok 1 - Preserve the zero-based transparent palette key after index rebasing"
exit 0
