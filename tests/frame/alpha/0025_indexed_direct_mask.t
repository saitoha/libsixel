#!/bin/sh
# Direct indexed output preserves a separate mask and its palette key.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical indexed-direct-mask || {
    echo 'not ok 1 - Direct indexed output preserves a separate mask and its palette key'
    exit 0
}
echo 'ok 1 - Direct indexed output preserves a separate mask and its palette key'
exit 0
