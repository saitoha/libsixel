#!/bin/sh
# Verify spec parse invalid.
set -eux
echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "assessment/0012_spec_parse_invalid" || {
    echo "not ok 1 - spec parse invalid"
    exit 0
}
echo "ok 1 - spec parse invalid"
exit 0
