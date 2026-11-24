#!/bin/sh
# Verify config size.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "progressive/0017_config_size" || {
    printf '\n%s\n' "not ok 1 - config size"
    exit 0
}
printf '\n%s\n' "ok 1 - config size"
exit 0
