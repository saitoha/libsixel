#!/bin/sh
# A profiled frame selects A2B1 when both forward tables are present.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical relative-lut || {
    echo 'not ok 1 - A profiled frame selects A2B1 when both forward tables are present'
    exit 0
}
echo 'ok 1 - A profiled frame selects A2B1 when both forward tables are present'
exit 0
