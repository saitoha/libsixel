#!/bin/sh
# Playback rejects a different transfer exponent in one channel.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation power blue reject || {
    printf '\n%s\n' 'not ok 1 - Playback rejects a different transfer exponent in one channel'
    exit 0
}
printf '\n%s\n' 'ok 1 - Playback rejects a different transfer exponent in one channel'
exit 0
