#!/bin/sh
# TAP runner for sixel_parse_header coverage cases.

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "probe/0001_probe_parse" 1>&2 || {
    printf '\n%s\n' "not ok 1 - probe_parse"
    exit 0
}

printf '\n%s\n' "ok 1 - probe_parse"

exit 0
