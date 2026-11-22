#!/bin/sh
# Verify that profiled frame resolves the ColorSpace A2B0 fallback.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical profile-lut || {
    echo 'not ok 1 - profiled frame resolves the ColorSpace A2B0 fallback'
    exit 0
}
echo 'ok 1 - profiled frame resolves the ColorSpace A2B0 fallback'
exit 0
