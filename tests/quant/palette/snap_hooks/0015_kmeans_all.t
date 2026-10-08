#!/bin/sh
# Reduced K-means palette executes the requested snap stages exactly once.
# Policy: docs/encoder/palette-construction/snap-policy/README.md
set -eux
echo "1..1"
set -v
${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "palette/snap_hooks/kmeans_all" || {
    echo "not ok 1 - Reduced K-means palette executes the requested snap stages exactly once"
    exit 0
}
echo "ok 1 - Reduced K-means palette executes the requested snap stages exactly once"
exit 0
