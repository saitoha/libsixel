#!/bin/sh
# SMPTE-C float samples convert to gamma sRGB RGB888 values and tags.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical smptec-float rgb888 || {
    echo 'not ok 1 - SMPTE-C float samples convert to gamma sRGB RGB888 values and tags'
    exit 0
}
echo 'ok 1 - SMPTE-C float samples convert to gamma sRGB RGB888 values and tags'
exit 0
