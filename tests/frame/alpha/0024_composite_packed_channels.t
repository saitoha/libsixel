#!/bin/sh
# Packed alpha composition preserves channel order and both endpoints.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical composite-channels || {
    echo 'not ok 1 - Packed alpha composition preserves channel order and both endpoints'
    exit 0
}
echo 'ok 1 - Packed alpha composition preserves channel order and both endpoints'
exit 0
