#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify failed linear conversion leaves every original source sample intact.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" cms/0044_linear_failure_atomic || {
    echo "not ok 1 - failed linear CMS apply changed original samples"
    exit 0
}
echo "ok 1 - failed linear CMS apply retains original samples"
exit 0
