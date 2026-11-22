#!/bin/sh
# RGB and indexed colors expand into BGR byte order.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical byte-bgr || {
    echo 'not ok 1 - RGB and indexed colors expand into BGR byte order'
    exit 0
}
echo 'ok 1 - RGB and indexed colors expand into BGR byte order'
exit 0
