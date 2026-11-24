#!/bin/sh
# Policy: docs/encoder/output/or-mode/README.md
# Check every decoded index across this vertical offset boundary.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "encoder-core/0022_ormode_offset_top_0" || {
    printf '\n%s\n' "not ok 1 - ormode_offset_top_0"
    exit 0
}

printf '\n%s\n' "ok 1 - ormode_offset_top_0"
exit 0
