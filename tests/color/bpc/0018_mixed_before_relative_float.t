#!/bin/sh
# Regression: mixed before relative float.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/0018_mixed_before_relative_float || {
    printf '\n%s\n' 'not ok 1 - mixed before relative float'
    exit 0
}
printf '\n%s\n' 'ok 1 - mixed before relative float'
exit 0
