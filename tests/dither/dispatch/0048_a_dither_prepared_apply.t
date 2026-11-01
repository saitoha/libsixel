#!/bin/sh
# Prepared settings govern the actual nearest-color query.
# Policy: docs/encoder/execution-map/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "dither/dispatch/0048_a_dither_prepared_apply" || {
    echo 'not ok 1 - 0048_a_dither_prepared_apply'
    exit 0
}
echo 'ok 1 - 0048_a_dither_prepared_apply'
exit 0
