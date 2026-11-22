#!/bin/sh
# Linear packed gray expands to encoded sRGB bytes.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical linear-packed || {
    echo 'not ok 1 - Linear packed gray expands to encoded sRGB bytes'
    exit 0
}
echo 'ok 1 - Linear packed gray expands to encoded sRGB bytes'
exit 0
