#!/bin/sh
set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_GIF_LOOP_FORCE_LOOP1_UNBOUNDED=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (gif loop force loop1 unbounded)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (gif loop force loop1 unbounded)"
exit 0
