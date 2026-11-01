#!/bin/sh
# Active solver snap blends in double precision before palette storage.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/double_blend" || {
    echo "not ok 1 - Active solver snap blends in double precision before palette storage"
    exit 0
}
echo "ok 1 - Active solver snap blends in double precision before palette storage"
exit 0
