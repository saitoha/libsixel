#!/bin/sh
# Inactive typed snap preserves original double coordinates.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/stage_excluded" || {
    printf '\n%s\n' "not ok 1 - Inactive typed snap preserves original double coordinates"
    exit 0
}
printf '\n%s\n' "ok 1 - Inactive typed snap preserves original double coordinates"
exit 0
