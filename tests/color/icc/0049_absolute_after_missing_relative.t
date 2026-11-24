#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0049_absolute_after_missing_relative || {
    printf '\n%s\n' "not ok 1 - absolute follows an unavailable relative intent"
    exit 0
}

printf '\n%s\n' "ok 1 - absolute follows an unavailable relative intent"
exit 0
