#!/bin/sh
# A later invalid coordinate leaves the complete float palette unchanged.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/final_rollback" || {
    echo "not ok 1 - A later invalid coordinate leaves the complete float palette unchanged"
    exit 0
}
echo "ok 1 - A later invalid coordinate leaves the complete float palette unchanged"
exit 0
