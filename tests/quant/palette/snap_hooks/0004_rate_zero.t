#!/bin/sh
# Inactive typed snap preserves original double coordinates.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/rate_zero" || {
    echo "not ok 1 - Inactive typed snap preserves original double coordinates"
    exit 0
}
echo "ok 1 - Inactive typed snap preserves original double coordinates"
exit 0
