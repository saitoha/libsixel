#!/bin/sh
# Playback rejects palette sources with different RGB primaries.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation display-p3 srgb reject || {
    printf '\n%s\n' 'not ok 1 - Playback rejects palette sources with different RGB primaries'
    exit 0
}
printf '\n%s\n' 'ok 1 - Playback rejects palette sources with different RGB primaries'
exit 0
