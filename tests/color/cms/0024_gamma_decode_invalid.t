#!/bin/sh
# Verify gamma decode invalid.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "cms/0019_gamma_decode_invalid" || {
    printf '\n%s\n' "not ok 1 - gamma decode invalid"
    exit 0
}
printf '\n%s\n' "ok 1 - gamma decode invalid"
exit 0
