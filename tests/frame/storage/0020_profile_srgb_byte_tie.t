#!/bin/sh
# Explicit sRGB rounds an exact half-byte tie and its neighbors correctly.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical srgb-byte-tie || {
    echo 'not ok 1 - explicit sRGB rounds neutral gray to RGB888'
    exit 0
}
echo 'ok 1 - explicit sRGB rounds neutral gray to RGB888'
exit 0
