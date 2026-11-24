#!/bin/sh
# Test-plan: docs/testing/builtin-loader-coverage.md
set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_GIF_LOOP_FORCE_LOOP2_UNBOUNDED=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    printf '\n%s\n' "not ok 1 - loader/0014_loader_builtin_pixelformat (gif loop force loop2 unbounded)"
    exit 0
}

printf '\n%s\n' "ok 1 - loader/0014_loader_builtin_pixelformat (gif loop force loop2 unbounded)"
exit 0
