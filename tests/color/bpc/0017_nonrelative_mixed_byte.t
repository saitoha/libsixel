#!/bin/sh
# Regression: nonrelative mixed byte.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/0017_nonrelative_mixed_byte || {
    printf '\n%s\n' 'not ok 1 - nonrelative mixed byte'
    exit 0
}
printf '\n%s\n' 'ok 1 - nonrelative mixed byte'
exit 0
