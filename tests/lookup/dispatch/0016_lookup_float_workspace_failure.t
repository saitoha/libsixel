#!/bin/sh
# Policy: docs/encoder/palette-application/lookup-policy/README.md

set -eux

echo "1..1"
set -v

${SIXEL_RUNTIME-} "${TEST_RUNNER_PATH}" "lookup/0020_float_workspace_failure" || {
    echo "not ok 1 - float workspace failure"
    exit 0
}
echo "ok 1 - float workspace failure"
exit 0
