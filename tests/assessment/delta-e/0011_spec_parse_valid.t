#!/bin/sh
# Verify spec parse valid.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0011_spec_parse_valid" || {
    printf '\n%s\n' "not ok 1 - spec parse valid"
    exit 0
}
printf '\n%s\n' "ok 1 - spec parse valid"
exit 0
