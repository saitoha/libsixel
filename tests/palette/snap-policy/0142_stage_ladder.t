#!/bin/sh
# Each timing value enables only its additive solver stages.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/stage_ladder" || {
    printf '\n%s\n' "not ok 1 - Each timing value enables only its additive solver stages"
    exit 0
}
printf '\n%s\n' "ok 1 - Each timing value enables only its additive solver stages"
exit 0
