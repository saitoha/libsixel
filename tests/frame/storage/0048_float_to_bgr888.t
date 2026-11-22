#!/bin/sh
# Float RGB converts to encoded BGR bytes.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical float-bgr || {
    echo 'not ok 1 - Float RGB converts to encoded BGR bytes'
    exit 0
}
echo 'ok 1 - Float RGB converts to encoded BGR bytes'
exit 0
