#!/bin/sh
# Full snap preserves target selection beside a huge source component.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/large_source" || {
    printf '\n%s\n' "not ok 1 - Full snap preserves target selection beside a huge source component"
    exit 0
}
printf '\n%s\n' "ok 1 - Full snap preserves target selection beside a huge source component"
exit 0
