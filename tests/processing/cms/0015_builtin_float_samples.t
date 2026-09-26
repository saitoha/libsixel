#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify builtin float samples.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0015_builtin_float_samples" || {
    echo "not ok 1 - builtin float samples"
    exit 0
}
echo "ok 1 - builtin float samples"
exit 0
