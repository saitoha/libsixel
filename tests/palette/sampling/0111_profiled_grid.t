#!/bin/sh
# Verify that adaptive grid sample retains Display P3 after source release.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" filter/0002_filter_sample profiled-grid || {
    printf '\n%s\n' 'not ok 1 - adaptive grid sample retains Display P3 after source release'
    exit 0
}
printf '\n%s\n' 'ok 1 - adaptive grid sample retains Display P3 after source release'
exit 0
