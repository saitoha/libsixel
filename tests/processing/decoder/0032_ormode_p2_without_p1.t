#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Distinguish this OR composition rule from ordinary painting.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "decoder/0032_ormode_p2_without_p1" || {
    echo "not ok 1 - ormode_p2_without_p1"
    exit 0
}

echo "ok 1 - ormode_p2_without_p1"
exit 0
