#!/bin/sh
# Policy: docs/functionality/or-mode.md
# Check every decoded index across this vertical offset boundary.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0025_ormode_offset_top_6" || {
    echo "not ok 1 - ormode_offset_top_6"
    exit 0
}

echo "ok 1 - ormode_offset_top_6"
exit 0
