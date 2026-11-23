#!/bin/sh
# Verify that adaptive grid sample retains Display P3 after source release.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" filter/0002_filter_sample profiled-grid || {
    echo 'not ok 1 - adaptive grid sample retains Display P3 after source release'
    exit 0
}
echo 'ok 1 - adaptive grid sample retains Display P3 after source release'
exit 0
