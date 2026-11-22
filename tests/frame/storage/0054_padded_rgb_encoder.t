#!/bin/sh
# Padded RGB frames match the RGB888 reference in each working mode.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical padded-rgb || {
    echo 'not ok 1 - Padded RGB frames match the RGB888 reference in each working mode'
    exit 0
}
echo 'ok 1 - Padded RGB frames match the RGB888 reference in each working mode'
exit 0
