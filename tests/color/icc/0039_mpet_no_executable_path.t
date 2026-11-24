#!/bin/sh

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" icc/0039_mpet_no_executable_path || {
    printf '\n%s\n' "not ok 1 - a parsed mpet cannot supply an executable path"
    exit 0
}

printf '\n%s\n' "ok 1 - a parsed mpet cannot supply an executable path"
exit 0
