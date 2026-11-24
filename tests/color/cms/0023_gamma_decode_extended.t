#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify gamma decode extended.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0018_gamma_decode_extended" || {
    printf '\n%s\n' "not ok 1 - gamma decode extended"
    exit 0
}
printf '\n%s\n' "ok 1 - gamma decode extended"
exit 0
