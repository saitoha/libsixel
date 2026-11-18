#!/bin/sh
# Resolve both RGB888 and BGR888 streams to the expected colors at every pixel.
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" filter/0010_filter_encode bgr888 || {
    echo 'not ok 1 - BGR888 and RGB888 encode the expected pixel colors'
    exit 0
}
echo 'ok 1 - BGR888 and RGB888 encode the expected pixel colors'
exit 0
