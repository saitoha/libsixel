#!/bin/sh
# Policy: docs/loader/color-management.md
# Verify gamma decode extended.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0018_gamma_decode_extended" || {
    echo "not ok 1 - gamma decode extended"
    exit 0
}
echo "ok 1 - gamma decode extended"
exit 0
