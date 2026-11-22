#!/bin/sh
# Unpacking linear palette indices preserves their coordinates.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical linear-unpack || {
    echo 'not ok 1 - Unpacking linear palette indices preserves their coordinates'
    exit 0
}
echo 'ok 1 - Unpacking linear palette indices preserves their coordinates'
exit 0
