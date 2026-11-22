#!/bin/sh
# Playback accepts independently constructed Display P3 definitions.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation display-p3 display-p3 accept || {
    echo 'not ok 1 - Playback accepts independently constructed Display P3 definitions'
    exit 0
}
echo 'ok 1 - Playback accepts independently constructed Display P3 definitions'
exit 0
