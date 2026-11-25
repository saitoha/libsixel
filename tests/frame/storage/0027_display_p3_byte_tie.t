#!/bin/sh
# Display P3 neutral samples round at the exact RGB888 boundary.
# Policy: docs/concepts/pixelformat-precision/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" frame/0014_clone_float_canonical p3-byte-tie || {
    printf '\n%s\n' 'not ok 1 - Display P3 neutral samples round at the exact RGB888 boundary'
    exit 0
}
printf '\n%s\n' 'ok 1 - Display P3 neutral samples round at the exact RGB888 boundary'
exit 0
