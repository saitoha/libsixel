#!/bin/sh
# Verify implicit linear image accepts explicit linear palette.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/0003_default_colorspace_auto \
    linear-palette 1 >&2 || {
    echo 'not ok 1 - implicit linear image accepts explicit linear palette'
    exit 0
}
echo 'ok 1 - implicit linear image accepts explicit linear palette'
exit 0
