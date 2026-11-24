#!/bin/sh
# Policy: docs/concepts/pixelformat-precision/README.md
# Verify frame clone float canonical.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "frame/0014_clone_float_canonical" || {
    printf '\n%s\n' "not ok 1 - frame clone float canonical"
    exit 0
}
printf '\n%s\n' "ok 1 - frame clone float canonical"
exit 0
