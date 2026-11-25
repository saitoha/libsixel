#!/bin/sh
# Decode a profiled float frame encoded with the default working spaces.
# Policy: docs/encoder/palette-application/working-colorspace/README.md
set -eux
echo '1..1'
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/0003_default_colorspace_auto display-p3 1>&2 || {
    printf '\n%s\n' 'not ok 1 - default working spaces preserve Display P3 pixel colors'
    exit 0
}
printf '\n%s\n' 'ok 1 - default working spaces preserve Display P3 pixel colors'
exit 0
