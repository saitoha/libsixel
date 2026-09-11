#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Check every decoded index across this vertical offset boundary.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0022_ormode_offset_top_0" || {
    echo "not ok 1 - ormode_offset_top_0"
    exit 0
}

echo "ok 1 - ormode_offset_top_0"
exit 0
