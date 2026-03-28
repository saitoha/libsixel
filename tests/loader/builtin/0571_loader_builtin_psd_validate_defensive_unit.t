#!/bin/sh
# Verify PSD validate defensive branches that are not reachable from file-level
# decode fixtures.

set -eux


echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    --env "SIXEL_TEST_PSD_VALIDATE_DEFENSIVE=1" \
    "loader/0014_loader_builtin_pixelformat" || {
    echo "not ok 1 - loader/0014_loader_builtin_pixelformat (psd validate defensive unit)"
    exit 0
}

echo "ok 1 - loader/0014_loader_builtin_pixelformat (psd validate defensive unit)"
exit 0
