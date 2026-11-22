#!/bin/sh
# Automatic GPU selection preserves terminal monochrome coverage.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical auto-mono || {
    echo 'not ok 1 - Automatic GPU selection preserves terminal monochrome coverage'
    exit 0
}
echo 'ok 1 - Automatic GPU selection preserves terminal monochrome coverage'
exit 0
