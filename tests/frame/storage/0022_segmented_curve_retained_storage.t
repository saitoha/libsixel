#!/bin/sh
# Segmented ICC curves release their breakpoint and segment arrays.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical segmented-storage || {
    echo 'not ok 1 - segmented curves retain only their sampled tables'
    exit 0
}
echo 'ok 1 - segmented curves retain only their sampled tables'
exit 0
