#!/bin/sh
# Policy: docs/loader/color-management/README.md
# Verify the finite libm model underlying black-lightness error bounds.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" colorspace/0013_black_log1p_model || {
    echo "not ok 1 - black log1p model"
    exit 0
}
echo "ok 1 - black log1p model"
exit 0
