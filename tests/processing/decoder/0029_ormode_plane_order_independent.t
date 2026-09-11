#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Distinguish this OR composition rule from ordinary painting.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0029_ormode_plane_order_independent" || {
    echo "not ok 1 - ormode_plane_order_independent"
    exit 0
}

echo "ok 1 - ormode_plane_order_independent"
exit 0
