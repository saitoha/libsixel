#!/bin/sh
# Regression: mixed before relative byte.
set -eux
echo '1..1'
set -v
SIXEL_LOADER_CMS_BPC=1 \
    ${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" bpc/0019_mixed_before_relative_byte || {
    echo 'not ok 1 - mixed before relative byte'
    exit 0
}
echo 'ok 1 - mixed before relative byte'
exit 0
