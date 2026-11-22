#!/bin/sh
# Byte resize quantizes linear shadows after applying the sRGB transfer.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical linear-dark-resize || {
    echo 'not ok 1 - Byte resize quantizes linear shadows after applying the sRGB transfer'
    exit 0
}
echo 'ok 1 - Byte resize quantizes linear shadows after applying the sRGB transfer'
exit 0
