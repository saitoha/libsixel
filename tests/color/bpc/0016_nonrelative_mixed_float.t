#!/bin/sh
# Regression: nonrelative mixed float.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/0016_nonrelative_mixed_float || {
    echo 'not ok 1 - nonrelative mixed float'
    exit 0
}
echo 'ok 1 - nonrelative mixed float'
exit 0
