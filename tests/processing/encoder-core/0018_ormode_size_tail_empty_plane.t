#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Check OR size-policy geometry and its distinct band branch.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0018_ormode_size_tail_empty_plane" || {
    echo "not ok 1 - ormode_size_tail_empty_plane"
    exit 0
}

echo "ok 1 - ormode_size_tail_empty_plane"
exit 0
