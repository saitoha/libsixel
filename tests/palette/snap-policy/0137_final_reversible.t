#!/bin/sh
# Typed partial snap blends extended coordinates toward independent targets.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/final_reversible" || {
    printf '\n%s\n' "not ok 1 - Typed partial snap blends extended coordinates toward independent targets"
    exit 0
}
printf '\n%s\n' "ok 1 - Typed partial snap blends extended coordinates toward independent targets"
exit 0
