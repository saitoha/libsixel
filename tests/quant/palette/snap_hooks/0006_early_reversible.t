#!/bin/sh
# Typed partial snap blends extended coordinates toward independent targets.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/early_reversible" || {
    echo "not ok 1 - Typed partial snap blends extended coordinates toward independent targets"
    exit 0
}
echo "ok 1 - Typed partial snap blends extended coordinates toward independent targets"
exit 0
