#!/bin/sh
# Verify compose gap ten.
set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" \
    "encoder-core/0046_compose_gap_ten" || {
    printf '\n%s\n' "not ok 1 - compose gap ten"
    exit 0
}

printf '\n%s\n' "ok 1 - compose gap ten"
exit 0
