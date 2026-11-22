#!/bin/sh
# D60 analytic RGB neutrals round at the exact RGB888 boundary.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical d60-byte-tie || {
    echo 'not ok 1 - D60 analytic RGB neutrals round at the exact RGB888 boundary'
    exit 0
}
echo 'ok 1 - D60 analytic RGB neutrals round at the exact RGB888 boundary'
exit 0
