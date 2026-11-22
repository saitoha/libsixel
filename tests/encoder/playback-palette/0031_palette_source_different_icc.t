#!/bin/sh
# Playback rejects same-size ICC profiles with different colorants.
# Policy: docs/encoder/palette-construction/pipeline/README.md
set -eux
echo '1..1'
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" planner/pipeline/0101_reserved_validation icc icc-swap reject || {
    echo 'not ok 1 - Playback rejects same-size ICC profiles with different colorants'
    exit 0
}
echo 'ok 1 - Playback rejects same-size ICC profiles with different colorants'
exit 0
