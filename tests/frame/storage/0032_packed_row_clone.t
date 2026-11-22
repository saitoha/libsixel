#!/bin/sh
# Packed frame clones copy exactly the padded rows.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical packed-clone || {
    echo 'not ok 1 - Packed frame clones copy exactly the padded rows'
    exit 0
}
echo 'ok 1 - Packed frame clones copy exactly the padded rows'
exit 0
