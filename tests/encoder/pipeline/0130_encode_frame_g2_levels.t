#!/bin/sh
# Frame encoding preserves every G2 gray level.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" filter/0010_filter_encode frame g2 || {
    echo 'not ok 1 - Frame encoding preserves every G2 gray level'
    exit 0
}
echo 'ok 1 - Frame encoding preserves every G2 gray level'
exit 0
