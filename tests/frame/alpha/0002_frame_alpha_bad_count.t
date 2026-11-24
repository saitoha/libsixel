#!/bin/sh
# Policy: docs/concepts/pixelformat.md
# Verify frame continuous alpha bad count.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0239_alpha" 1 || {
    printf '\n%s\n' "not ok 1 - frame continuous alpha bad count"
    exit 0
}
printf '\n%s\n' "ok 1 - frame continuous alpha bad count"
exit 0
