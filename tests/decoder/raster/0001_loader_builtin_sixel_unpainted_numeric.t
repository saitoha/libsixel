#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/sixel.md
# Distinguish an unpainted index before and after palette expansion.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "loader/0118_loader_builtin_sixel_unpainted_numeric" || {
    printf '\n%s\n' "not ok 1 - SIXEL unpainted sentinel representation"
    exit 0
}

printf '\n%s\n' "ok 1 - SIXEL unpainted sentinel representation"
exit 0
