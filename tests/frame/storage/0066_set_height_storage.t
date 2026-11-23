#!/bin/sh
# Setting height retains the original pixel footprint.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    frame/0014_clone_float_canonical height-storage || {
    echo 'not ok 1 - Setting height retains the original pixel footprint'
    exit 0
}
echo 'ok 1 - Setting height retains the original pixel footprint'
exit 0
