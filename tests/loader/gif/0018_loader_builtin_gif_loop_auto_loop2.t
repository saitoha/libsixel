#!/bin/sh
# Policy: docs/encoder/animation.md
# Test-plan: docs/testing/builtin-loader-coverage.md
# Policy: docs/loader/builtin/gif.md
set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_GIF_LOOP_AUTO_LOOP2_TWICE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    printf '\n%s\n' "not ok 1 - loader/0014_loader_builtin_pixelformat (gif loop auto loop2 twice)"
    exit 0
}

printf '\n%s\n' "ok 1 - loader/0014_loader_builtin_pixelformat (gif loop auto loop2 twice)"
exit 0
